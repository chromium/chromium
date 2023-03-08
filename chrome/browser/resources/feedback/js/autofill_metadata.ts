// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {createLogsMapTable} from './logs_map_page.js';

const dialogArgs: string = chrome.getVariableValue('dialogArguments');

/**
 * Builds the autofill metadata table.
 */
function createAutofillMetadataTable() {
  // TODO(crbug.com/1407646): Build autofill metadata table entries.
  createLogsMapTable([]);
}

/**
 * Initializes the page when the window is loaded.
 */
window.onload = function() {
  if (!dialogArgs) {
    return;
  }

  createAutofillMetadataTable();
};
