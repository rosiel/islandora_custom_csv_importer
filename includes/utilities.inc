<?php

/**
 * @file
 * Creates Islandora Entity Objects from CSV.
 */

/**
 * Created Entities from supplied csv file.
 *
 * @param url $path
 *   Path to file
 */
function islandora_custom_import_csv_batch($path) {
  $objects = islandora_custom_read_csv($path);
  $batch = array(
    'title' => t("Creating Custom Objects"),
    'progress_message' => t('Adding @current object out of @total.'),
    'operations' => array(),
    'file' => drupal_get_path('module', 'islandora_custom_csv_import') . '/includes/utilities.inc',
  );
  foreach ($objects as $object) {
    $batch['operations'][] = array(
      'islandora_custom_csv_import_build_object',
      array($object),
    );
  }
  batch_set($batch);
}

/**
 * Creates associative array representing scholar data.
 *
 * @param url $path
 *   Path to CSV file
 *
 * @return array
 *   array of associative arrays containing header => value pairs
 */
function islandora_custom_read_csv($path) {
  ini_set("auto_detect_line_endings", "1");
  $objects = array();
  if (($handle = fopen($path, "r")) !== FALSE) {
    while (($data = fgetcsv($handle, 0, ",")) !== FALSE) {
      if (!isset($header)) {
        $header = $data;
        continue;
      }
      for ($counter = 0; $counter < count($data); $counter++) {
        $object[$header[$counter]] = $data[$counter];
      }
      $objects[] = $object;
    }
    fclose($handle);
  }
  return $objects;
}

/**
 * Builds  objects.
 *
 * @param array $object
 *   Header => value pairs representing date to be converted to MADS record
 * @param string $collection_pid
 *   PID of collection to hold new objects.
 */
function islandora_custom_csv_import_build_object($object) {

  // Custom stuff:
  $object['PID'] = 'bdh';
  $object['CMODEL'] = 'islandora:sp_large_image_cmodel';
  $filepath = htmlspecialchars(str_replace('||', ',', $object['PATH']));
  $path_parts = explode("/", $filepath);
  $filename = array_pop($path_parts);
  $object['FOLDER'] = array_pop($path_parts);
  $object['LABEL'] = "{$object['FOLDER']} - $filename";
  $object['MHID'] = preg_replace('/-Dm[-.].*$/', '', $filename);


  $tuque = new IslandoraTuque();
  $repository = $tuque->repository;
  $fedora_object = $repository->constructObject($object['PID']);
  $fedora_object->label = isset($object['LABEL']) ? htmlspecialchars(str_replace('||', ',', $object['LABEL'])) : 'Label';
  $fedora_object->owner = 'admin';
  $cmodels = explode('~', $object['CMODEL']);
  foreach ($cmodels as $cmodel) {
    $fedora_object->relationships->add(FEDORA_MODEL_URI, 'hasModel', $cmodel);
  }
  $parents = explode('~', $object['PARENT']);
  foreach ($parents as $parent) {
    $fedora_object->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOfCollection', $parent);
  }

  $mods = islandora_custom_csv_import_build_mods($object);
  $datastream = $fedora_object->constructDatastream('MODS', 'M');
  $datastream->label = 'MODS';
  $datastream->mimetype = 'application/xml';
  $datastream->content = $mods;
  $fedora_object->ingestDatastream($datastream);

  $new_fedora_object = islandora_add_object($fedora_object);
  file_put_contents('public://rosielog.txt', "{$fedora_object->id}\t{$filepath}\n", FILE_APPEND);

}

/**
 * Builds MADS stream from Scholar data.
 *
 * @param array $object
 *   Associative array of header => value pairs
 *
 * @return xml
 *   MADS xml representing individual scholar
 */
function islandora_custom_csv_import_build_mods($object) {

  // ---
  $dom = new DomDocument('1.0', 'UTF-8');
  $dom->preserveWhiteSpace = FALSE;
  $dom->formatOutput = TRUE;
  $root = $dom->createElement('mods');
  $dom->appendChild($root);

  $mods_uri = "http://www.loc.gov/mods/v3";
  $ns_mods = $dom->createAttribute('xmlns');
  $root->appendChild($ns_mods);
  $value = $dom->createTextNode($mods_uri);
  $ns_mods->appendChild($value);

  $ns_mods_qualified = $dom->createAttribute('xmlns:mods');
  $root->appendChild($ns_mods_qualified);
  $value = $dom->createTextNode($mods_uri);
  $ns_mods_qualified->appendChild($value);

  $xsi = $dom->createAttribute('xmlns:xsi');
  $root->appendChild($xsi);
  $value = $dom->createTextNode("http://www.w3.org/2001/XMLSchema-instance");
  $xsi->appendChild($value);

  $xlink = $dom->createAttribute('xmlns:xlink');
  $root->appendChild($xlink);
  $value = $dom->createTextNode("http://www.w3.org/1999/xlink");
  $xlink->appendChild($value);
  // ---


  // Add title of image.
  $titleInfo = $dom->createElement('titleInfo');
  $root->appendChild($titleInfo);
  $title = $dom->createElement('title', islandora_custom_csv_import_clean_for_xml($object['LABEL']));
  $titleInfo->appendChild($title);


  // Add identifier.
  $identifier = $dom->createElement('identifier', $object["MHID"]);
  $identifier->setAttribute('type', 'local');
  $root->appendChild($identifier);

  // Add Museum ID.
  $identifier = $dom->createElement('identifier', $object["MHID"]);
  $identifier->setAttribute('type', 'CMH');
  $root->appendChild($identifier);

  // Add source.
  $note = $dom->createElement('note', islandora_custom_csv_import_clean_for_xml($object['FOLDER']));
  $root->appendChild($note);

  // Add type.
  $type = $dom->createElement('typeOfResource', 'still image');
  $root->appendChild($type);

  // Add form.
  $pdesc = $dom->createElement('physicalDescription');
  $root->appendChild($pdesc);
  $form = $dom->createElement('form', 'nonprojected graphic');
  $form->setAttribute('authority', 'marcform');
  $pdesc->appendChild($form);


  return $dom->saveXML();
}

/**
 * Cleans a string for xml.
 */
function islandora_custom_csv_import_clean_for_xml($string) {
  $string = trim($string);
  $string = str_replace('&', '&amp;', $string);
  return $string;
}
