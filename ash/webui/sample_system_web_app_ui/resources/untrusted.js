// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var header = document.getElementById('untrusted-title');
header.textContent = 'Untrusted Sample System Web App';

// For testing purposes: notify the parent window the iframe has been embedded
// successfully.
window.addEventListener('message', event => {
  if (event.origin.startsWith('chrome://sample-system-web-app')) {
    window.parent.postMessage(
        {'success': true}, 'chrome://sample-system-web-app');
  }
});
