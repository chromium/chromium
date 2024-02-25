// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './pdf_viewer_wrapper.js';

import type {BrowserApi} from './browser_api.js';
import {createBrowserApi} from './browser_api.js';

/**
 * Stores any pending messages received which should be passed to the
 * PDFViewer when it is created.
 */
const pendingMessages: MessageEvent[] = [];

/**
 * Handles events that are received prior to the PDFViewer being created.
 */
function handleScriptingMessage(message: MessageEvent) {
  pendingMessages.push(message);
}

/**
 * Initialize the global PDFViewer and pass any outstanding messages to it.
 */
function initViewer(browserApi: BrowserApi) {
  // PDFViewer will handle any messages after it is created.
  window.removeEventListener('message', handleScriptingMessage, false);
  const viewer = document.querySelector('pdf-viewer')!;
  viewer.init(browserApi);
  while (pendingMessages.length > 0) {
    viewer.handleScriptingMessage(pendingMessages.shift()!);
  }
  Object.assign(window, {viewer});
}

/**
 * Determine if the content settings allow PDFs to execute javascript.
 */
function configureJavaScriptContentSetting(browserApi: BrowserApi):
    Promise<BrowserApi> {
  return new Promise(resolve => {
    chrome.contentSettings.javascript.get(
        {
          'primaryUrl': browserApi.getStreamInfo().originalUrl,
          'secondaryUrl': window.location.origin,
        },
        (result) => {
          browserApi.getStreamInfo().javascript = result.setting;
          resolve(browserApi);
        });
  });
}

/**
 * Entrypoint for starting the PDF viewer. This function obtains the browser
 * API for the PDF and initializes the PDF Viewer.
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
