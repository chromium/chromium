// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

if (chrome.test) {
  chrome.test.onMessage.addListener((msg) => {
    if (msg.data != 'preloadZip')
      return;

    console.info('Preloading NaCl module');
    unpacker.app.loadNaclModule(
        unpacker.app.DEFAULT_MODULE_NMF, unpacker.app.DEFAULT_MODULE_TYPE);
  });
  chrome.test.sendMessage(JSON.stringify({name: 'zipArchiverLoaded'}));
}

function setupZipArchiver() {
  chrome.fileSystemProvider.onUnmountRequested.addListener(
      unpacker.app.onUnmountRequested);
  chrome.fileSystemProvider.onGetMetadataRequested.addListener(
      unpacker.app.onGetMetadataRequested);
  chrome.fileSystemProvider.onReadDirectoryRequested.addListener(
      unpacker.app.onReadDirectoryRequested);
  chrome.fileSystemProvider.onOpenFileRequested.addListener(
      unpacker.app.onOpenFileRequested);
  chrome.fileSystemProvider.onCloseFileRequested.addListener(
      unpacker.app.onCloseFileRequested);
  chrome.fileSystemProvider.onReadFileRequested.addListener(
      unpacker.app.onReadFileRequested);

  // Load translations
  unpacker.app.loadStringData();

  // Clean all temporary files inside the work directory, just in case the
  // extension aborted previously without removing ones.
  unpacker.app.cleanWorkDirectory();
}

// Event called on opening a file with the extension or mime type
// declared in the manifest file.
chrome.app.runtime.onLaunched.addListener(unpacker.app.onLaunched);

// Avoid handling events duplicatedly if this is in incognito context in a
// regular session. https://crbug.com/833603
// onLaunched must be registered without waiting for the profile to resolved,
// or otherwise it misses the first onLaunched event sent right after the
// extension is loaded. https://crbug.com/837251
chrome.fileManagerPrivate.getProfiles((profiles) => {
  if ((profiles[0] && profiles[0].profileId == '$guest') ||
      !chrome.extension.inIncognitoContext) {
    setupZipArchiver();
  } else {
    console.info(
        'The extension was silenced ' +
        'because this is in the incognito context of a regular session.');
  }
});
