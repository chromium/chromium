// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The background script for the tts engine.
 */

// TODO(crbug.com/353972727): Download the web assembly file of the Google TTS
// engine.
console.info('TTS engine extension installed!');

// This should be incremented each time there's a new upload of the TTS engine.
const CURRENT_VERSION = 'v1';
const DOWNLOAD_URL =
    'https://dl.google.com/android/tts/wasm/' + CURRENT_VERSION;


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
