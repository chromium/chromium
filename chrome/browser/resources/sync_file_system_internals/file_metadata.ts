// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * WebUI to monitor File Metadata per Extension ID.
 */

import {assert} from 'chrome://resources/js/assert.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {getImage} from 'chrome://resources/js/icon.js';

import {createElementFromDictionary, createElementFromText} from './utils.js';

function getSelect(): HTMLSelectElement {
  const select =
      document.querySelector<HTMLSelectElement>('#extensions-select');
  assert(select);
  return select;
}

/**
 * Gets extension data so the select drop down can be filled.
 */
function refreshExtensions() {
  sendWithPromise('getExtensions').then(onGetExtensions);
}

/**
 * Renders result of getFileMetadata as a table.
 */
function onGetExtensions(extensionStatuses: Array<{
  extensionName: string,
  extensionID: string,
  status: string,
}>) {
  const select = getSelect();

  // Record existing drop down extension ID. If it's still there after the
  // refresh then keep it as the selected value.
  const oldSelectedExtension = getSelectedExtensionId();

  select.textContent = '';
  for (let i = 0; i < extensionStatuses.length; i++) {
    const originEntry = extensionStatuses[i]!;
    const title = originEntry.extensionName + ' [' + originEntry.status + ']';
    select.options.add(new Option(title, originEntry.extensionID));

    // If option was the previously only selected, make it selected again.
    if (originEntry.extensionID !== oldSelectedExtension) {
      continue;
    }
    select.options[select.options.length - 1]!.selected = true;
  }

  // After drop down has been loaded with options, file metadata can be loaded
  refreshFileMetadata();
}

/**
 * @return extension ID that's currently selected in drop down box.
 */
function getSelectedExtensionId(): string|null {
  const select = getSelect();
  if (select.selectedIndex >= 0) {
    return select.options[select.selectedIndex]!.value;
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
    const header = document.querySelector<HTMLElement>('#file-metadata-header');
    assert(header);
    header.textContent = '';
    const entries =
        document.querySelector<HTMLElement>('#file-metadata-entries');
    assert(entries);
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
function onGetFileMetadata(fileMetadataMap: Array<{
  type: string,
  status: string,
  path: string,
  details: {[key: string]: string},
}>) {
  const header = document.querySelector<HTMLElement>('#file-metadata-header');
  assert(header);
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
  const itemContainer =
      document.querySelector<HTMLElement>('#file-metadata-entries');
  assert(itemContainer);
  itemContainer.textContent = '';
  for (let i = 0; i < fileMetadataMap.length; i++) {
    const metadatEntry = fileMetadataMap[i]!;
    const tr = document.createElement('tr');
    tr.appendChild(createFileIconCell(metadatEntry.type));
    tr.appendChild(createElementFromText('td', metadatEntry.status));
    tr.appendChild(createElementFromText('td', metadatEntry.path));
    tr.appendChild(createElementFromDictionary('td', metadatEntry.details));
    itemContainer.appendChild(tr);
  }
}

/**
 * @param type file type string.
 * @return TD with file or folder icon depending on type.
 */
function createFileIconCell(type: string): HTMLElement {
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

  const td = document.createElement('td');
  td.className = 'file-icon-cell';
  td.appendChild(imgWrapper);
  td.appendChild(document.createTextNode(type));
  return td;
}

function main() {
  refreshExtensions();
  const refresh =
      document.querySelector<HTMLElement>('#refresh-metadata-button');
  assert(refresh);
  refresh.addEventListener('click', refreshExtensions);
  getSelect().addEventListener('change', refreshFileMetadata);
}

document.addEventListener('DOMContentLoaded', main);
