// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './elements/viewer-error-screen.js';
import './elements/viewer-page-indicator.js';
import './elements/viewer-password-screen.js';
import './elements/viewer-pdf-toolbar.js';
import './elements/viewer-zoom-toolbar.js';
import './elements/shared-vars.js';
// <if expr="chromeos">
import './elements/viewer-ink-host.js';
import './elements/viewer-form-warning.js';
// </if>

import {BrowserApi, createBrowserApi} from './browser_api.js';
import {PDFViewer} from './pdf_viewer.js';

/**
 * Global PDFViewer object, accessible for testing.
 *
 * @type Object
 */
window.viewer = null;


/**
 * Stores any pending messages received which should be passed to the
 * PDFViewer when it is created.
 *
 * @type Array
 */
const pendingMessages = [];

/**
 * Handles events that are received prior to the PDFViewer being created.
 *
 * @param {Object} message A message event received.
 */
function handleScriptingMessage(message) {
  pendingMessages.push(message);
}

/**
 * Initialize the global PDFViewer and pass any outstanding messages to it.
 *
 * @param {!BrowserApi} browserApi
 */
function initViewer(browserApi) {
  // PDFViewer will handle any messages after it is created.
  window.removeEventListener('message', handleScriptingMessage, false);
  window.viewer = new PDFViewer(browserApi);
  while (pendingMessages.length > 0) {
    window.viewer.handleScriptingMessage(pendingMessages.shift());
  }
}

/**
 * Determine if the content settings allow PDFs to execute javascript.
 *
 * @param {!BrowserApi} browserApi
 * @return {!Promise<!BrowserApi>}
 */
function configureJavaScriptContentSetting(browserApi) {
  return new Promise((resolve, reject) => {
    chrome.contentSettings.javascript.get(
        {
          'primaryUrl': browserApi.getStreamInfo().originalUrl,
          'secondaryUrl': window.location.origin
        },
        (result) => {
          browserApi.getStreamInfo().javascript = result.setting;
          resolve(browserApi);
        });
  });
}

/**
 * Entrypoint for starting the PDF viewer. This function obtains the browser
 * API for the PDF and constructs a PDFViewer object with it.
 */
function main() {
  // Set up an event listener to catch scripting messages which are sent prior
  // to the PDFViewer being created.
  window.addEventListener('message', handleScriptingMessage, false);
  let chain = createBrowserApi();

  // Content settings may not be present in test environments.
  if (chrome.contentSettings) {
    chain = chain.then(configureJavaScriptContentSetting);
  }

  chain.then(initViewer);
}

main();
