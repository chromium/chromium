// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Setup handlers for the minimize and close topbar buttons.
 */
function assistantInit() {
  const closeButton = $('assistant-close-button');
  closeButton.addEventListener('mousedown', function(e) {
    e.preventDefault();
  });
  closeButton.addEventListener('click', function() {
    chrome.app.window.current().close();
  });
  window.addEventListener('keydown', (event) => {
    if (event.key === 'Escape') {
      chrome.app.window.current().close();
    }
  }, false);
}
window.addEventListener('DOMContentLoaded', assistantInit);
