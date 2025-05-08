// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The background script for the tts engine.
 */

// TODO(crbug.com/353972727): Download the web assembly file of the Google TTS
// engine.
console.info('TTS engine extension installed!');

const DOWNLOAD_BASE_URL = 'https://dl.google.com/android/tts/wasm/';

// This should be incremented each time there's a new upload of the TTS engine.
let CURRENT_VERSION_NUMBER = 1.7;

fetch(DOWNLOAD_BASE_URL + 'version.json')
    .then(response => response.json())
    .then(data => {
      const version = data.versionNumber;
      if (version && typeof version === 'number') {
        // If the version is valid and less than the current version number,
        // rollback.
        if (version < CURRENT_VERSION_NUMBER) {
          // If the version is valid, update the download version.
          CURRENT_VERSION_NUMBER = version;
          refreshVersions();
        }
      } else {
        if (!version) {
          console.info(
              'Found invalid version in version.json: ' +
              'versionNumber undefined');
        } else {
          console.info('Found invalid version in version.json: ' + version);
        }
      }

      // Fetch the engine after we finish fetching version.json to ensure
      // we're using the correct engine.
      fetchEngine();
    });

let CURRENT_VERSION = 'v' + CURRENT_VERSION_NUMBER;

let DOWNLOAD_URL = DOWNLOAD_BASE_URL + CURRENT_VERSION;

function refreshVersions() {
  CURRENT_VERSION = 'v' + CURRENT_VERSION_NUMBER;
  DOWNLOAD_URL = DOWNLOAD_BASE_URL + CURRENT_VERSION;
}


function fetchEngine() {
  // TODO(crbug.com/353972727): Use of remotely fetched code is a temporary
  // solution until updating to use component updater.
  fetch(DOWNLOAD_URL + '/googletts_engine_js_bin.js')
      .then(response => response.text())
      .then(scriptContent => {
        const script = document.createElement('script');

        script.type = 'text/javascript';
        script.textContent = scriptContent;
        script.async = true;
        document.head.appendChild(script);
      })
      .catch(error => {
        console.info('Failed to fetch TTS engine: ' + error);
      });
}
