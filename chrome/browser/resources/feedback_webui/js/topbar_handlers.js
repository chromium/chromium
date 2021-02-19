// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Setup handlers for the minimize and close topbar buttons.
 */
function initializeHandlers() {
  // If this dialog is using system window controls, these elements aren't
  // needed at all.
  if (window.feedbackInfo.useSystemWindowFrame) {
    $('minimize-button').hidden = true;
    $('close-button').hidden = true;
    return;
  }
  $('minimize-button').addEventListener('click', function(e) {
    e.preventDefault();
    chrome.app.window.current().minimize();
  });

  $('minimize-button').addEventListener('mousedown', function(e) {
    e.preventDefault();
  });

  $('close-button').addEventListener('click', function() {
    scheduleWindowClose();
  });

  $('close-button').addEventListener('mousedown', function(e) {
    e.preventDefault();
  });
}

window.addEventListener('DOMContentLoaded', initializeHandlers);
