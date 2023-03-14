// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createLogsMapTable} from './logs_map_page.js';

function getSystemInformation():
    Promise<chrome.feedbackPrivate.LogsMapEntry[]> {
  return new Promise(
      resolve => chrome.feedbackPrivate.getSystemInformation(resolve));
}

/**
 * Initializes the page when the window is loaded.
 */
window.onload = function() {
  getSystemInformation().then(createLogsMapTable);
};
