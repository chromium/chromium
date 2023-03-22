// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

showBlockingExtensions = (extensionNames) => {
  const ul = document.getElementById('blocking-extensions');
  for (extName of extensionNames) {
    const li = document.createElement('li');
    li.appendChild(document.createTextNode(extName));
    ul.appendChild(li);
  }
};

showMissingExtensions = (extensionIDs) => {
  const ul = document.getElementById('mising-extensions');
  for (extName of extensionIDs) {
    const li = document.createElement('li');
    li.appendChild(document.createTextNode(extName));
    ul.appendChild(li);
  }
};

/**
 * Initializes the page when the window is loaded.
 */
window.onload = () => {
  const missingExtensions = window.loadTimeData.getValue('missingExtensions');
  if (missingExtensions.length > 0) {
    document.getElementById('container-missing-extensions').hidden = false;
    showMissingExtensions(missingExtensions);
    return;
  }
  document.getElementById('container-blocking-extensions').hidden = false;
  showBlockingExtensions(window.loadTimeData.getValue('blockingExtensions'));
};