// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * WebUI to monitor File Metadata per Extension ID.
 */

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {getImage} from 'chrome://resources/js/icon.js';

import {createElementFromDictionary, createElementFromText} from './utils.js';

/** @return {!HTMLSelectElement} */
function getSelect() {
  return /** @type {!HTMLSelectElement} */ (
      document.querySelector('#extensions-select'));
}

/**
 * Gets extension data so the select drop down can be filled.
 */
function refreshExtensions() {
  sendWithPromise('getExtensions').then(onGetExtensions);
}

/**
 * Renders result of getFileMetadata as a table.
 * @param {!Array<!{
 *   extensionName: string,
 *   extensionID: string,
 *   status: string,
 * }>} extensionStatuses of dictionaries containing 'extensionName',
 *     'extensionID', 'status'.
 */
function onGetExtensions(extensionStatuses) {
  const select = getSelect();

  // Record existing drop down extension ID. If it's still there after the
  // refresh then keep it as the selected value.
  const oldSelectedExtension = getSelectedExtensionId();

  select.textContent = '';
  for (let i = 0; i < extensionStatuses.length; i++) {
    const originEntry = extensionStatuses[i];
    const tr = document.createElement('tr');
    const title = originEntry.extensionName + ' [' + originEntry.status + ']';
    select.options.add(new Option(title, originEntry.extensionID));

    // If option was the previously only selected, make it selected again.
    if (originEntry.extensionID !== oldSelectedExtension) {
      continue;
    }
    select.options[select.options.length - 1].selected = true;
  }

  // After drop down has been loaded with options, file metadata can be loaded
  refreshFileMetadata();
}

/**
 * @return {?string} extension ID that's currently selected in drop down box.
 */
function getSelectedExtensionId() {
  const select = getSelect();
  if (select.selectedIndex >= 0) {
    return select.options[select.selectedIndex].value;
  }

  return null;
}

/**
 * Get File Metadata depending on which extension is selected from the drop
 * down if any.
 */
function refreshFileMetadata() {
  const dropDown = getSelect();
  if (dropDown.options.length === 0) {
    const header = document.querySelector('#file-metadata-header');
    header.textContent = '';
    const entries = document.querySelector('#file-metadata-entries');
    entries.textContent = 'No file metadata available.';
    return;
  }

  const selectedExtensionId = getSelectedExtensionId();
  sendWithPromise('getFileMetadata', selectedExtensionId)
      .then(onGetFileMetadata);
}

/**
 * Renders result of getFileMetadata as a table.
 */
function onGetFileMetadata(fileMetadataMap) {
  const header = document.querySelector('#file-metadata-header');
  // Only draw the header if it hasn't been drawn yet
  if (header.children.length === 0) {
    const tr = document.createElement('tr');
    tr.appendChild(createElementFromText('td', 'Type'));
    tr.appendChild(createElementFromText('td', 'Status'));
    tr.appendChild(createElementFromText('td', 'Path', {width: '250px'}));
    tr.appendChild(createElementFromText('td', 'Details'));
    header.appendChild(tr);
  }

  // Add row entries.
  const itemContainer = document.querySelector('#file-metadata-entries');
  itemContainer.textContent = '';
  for (let i = 0; i < fileMetadataMap.length; i++) {
    const metadatEntry = fileMetadataMap[i];
    const tr = document.createElement('tr');
    tr.appendChild(createFileIconCell(metadatEntry.type));
    tr.appendChild(createElementFromText('td', metadatEntry.status));
    tr.appendChild(createElementFromText('td', metadatEntry.path));
    tr.appendChild(createElementFromDictionary('td', metadatEntry.details));
    itemContainer.appendChild(tr);
  }
}

/**
 * @param {string} type file type string.
 * @return {!HTMLElement} TD with file or folder icon depending on type.
 */
function createFileIconCell(type) {
  const img = document.createElement('div');
  const lowerType = type.toLowerCase();
  if (lowerType === 'file') {
    img.style.content = getImage('chrome://theme/IDR_DEFAULT_FAVICON');
  } else if (lowerType === 'folder') {
    img.style.content = getImage('chrome://theme/IDR_FOLDER_CLOSED');
    img.className = 'folder-image';
  }

  const imgWrapper = document.createElement('div');
  imgWrapper.appendChild(img);

  const td = /** @type {!HTMLElement} */ (document.createElement('td'));
  td.className = 'file-icon-cell';
  td.appendChild(imgWrapper);
  td.appendChild(document.createTextNode(type));
  return td;
}

function main() {
  refreshExtensions();
  const refresh = document.querySelector('#refresh-metadata-button');
  refresh.addEventListener('click', refreshExtensions);
  getSelect().addEventListener('change', refreshFileMetadata);
}

document.addEventListener('DOMContentLoaded', main);
