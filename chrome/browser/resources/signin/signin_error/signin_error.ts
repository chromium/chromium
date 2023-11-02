/* Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

function initialize() {
  // Prefer using |document.body.offsetHeight| instead of
  // |document.body.scrollHeight| as it returns the correct height of the
  // even when the page zoom in Chrome is different than 100%.
  chrome.send('initializedWithSize', [document.body.offsetHeight]);
}

document.addEventListener('DOMContentLoaded', initialize);
