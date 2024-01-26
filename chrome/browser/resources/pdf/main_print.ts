// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Entry point to the Print Preview PDF Viewer UI.

import './pdf_print_wrapper.js';

import type {BrowserApi} from './browser_api.js';
import {createBrowserApiForPrintPreview} from './browser_api.js';

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
  // JS always blocked in Print Preview.
  browserApi.getStreamInfo().javascript = 'block';
  // PDFViewer will handle any messages after it is created.
  window.removeEventListener('message', handleScriptingMessage, false);
  const viewer = document.querySelector('pdf-viewer-print')!;
  viewer.init(browserApi);
  while (pendingMessages.length > 0) {
    viewer.handleScriptingMessage(pendingMessages.shift()!);
  }
  Object.assign(window, {viewer});
}

/**
 * Entrypoint for starting the PDF viewer. This function obtains the browser
 * API for the PDF and initializes the PDF Viewer.
 */
function main() {
  // Set up an event listener to catch scripting messages which are sent prior
  // to the PDFViewer being created.
  window.addEventListener('message', handleScriptingMessage, false);
  createBrowserApiForPrintPreview().then(initViewer);
}

main();
