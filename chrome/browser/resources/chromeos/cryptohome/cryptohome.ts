// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addWebUiListener} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

/**
 * Sets |value| to the element specified by |destinationId|.
 * @param destinationId Id of the element to be modified.
 * @param value The value to be set.
 */
function onSetCryptohomeProperty(destinationId: string, value: string) {
  getRequiredElement(destinationId).textContent = value;
}

document.addEventListener('DOMContentLoaded', function() {
  // Request update.
  chrome.send('pageLoaded');

  addWebUiListener('SetCryptohomeProperty', onSetCryptohomeProperty);

  // Auto-refresh when interval is given as pathname.
  const pathPart = window.location.pathname.split('/')[1];
  const interval = pathPart ? parseInt(pathPart, 10) : 0;
  if (interval > 0) {
    getRequiredElement('refresh-message').textContent =
        '(Auto-refreshing page every ' + interval + 's)';
    setTimeout(function() {
      window.location.reload();
    }, interval * 1000);
  }
});
