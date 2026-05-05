// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addWebUiListener} from 'chrome://resources/js/cr.js';
import {$} from 'chrome://resources/js/util.js';

/**
 * Sets |value| to the element specified by |destination_id|.
 * @param {string} destination_id Id of the element to be modified.
 * @param {string} value The value to be set.
 */
function onSetCryptohomeProperty(destinationId, value) {
  $(destinationId).textContent = value;
}

document.addEventListener('DOMContentLoaded', function() {
  // Request update.
  chrome.send('pageLoaded');

  addWebUiListener('SetCryptohomeProperty', onSetCryptohomeProperty);

  // Auto-refresh when interval is given as pathname.
  const interval = parseInt(window.location.pathname.split('/')[1]);
  if (interval > 0) {
    $('refresh-message').textContent =
        '(Auto-refreshing page every ' + interval + 's)';
    setTimeout(function() {
      window.location.reload(true);
    }, interval * 1000);
  }
});
