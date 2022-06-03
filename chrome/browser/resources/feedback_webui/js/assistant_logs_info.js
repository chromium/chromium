// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';

/**
 * Setup handlers for the minimize and close topbar buttons.
 */
function assistantInit() {
  const closeButton = $('assistant-close-button');
  closeButton.addEventListener('mousedown', function(e) {
    e.preventDefault();
  });
  closeButton.addEventListener('click', function() {
    window.close();
  });
  window.addEventListener('keydown', (event) => {
    if (event.key === 'Escape') {
      window.close();
    }
  }, false);
}
window.addEventListener('DOMContentLoaded', assistantInit);
