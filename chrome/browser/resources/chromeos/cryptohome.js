// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Sets |value| to the element specified by |destination_id|.
 * Called from C++ code as a result of RequestCryptohomeProperty() call.
 * @param {string} destination_id Id of the element to be modified.
 * @param {string} value The value to be set.
 */
function SetCryptohomeProperty(destination_id, value) {
  $(destination_id).textContent = value;
}

document.addEventListener('DOMContentLoaded', function() {
  // Request update.
  chrome.send('pageLoaded');

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
