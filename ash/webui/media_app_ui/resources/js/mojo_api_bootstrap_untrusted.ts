// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MahiUntrustedPageCallbackRouter, MahiUntrustedPageHandlerRemote, OcrUntrustedPageCallbackRouter, OcrUntrustedPageHandlerRemote, UntrustedPageHandlerFactory} from './media_app_ui_untrusted.mojom-webui.js';

// Used to make calls on the remote OcrUntrustedPageHandler interface. Singleton
// that client modules can use directly.
let ocrUntrustedPageHandler: OcrUntrustedPageHandlerRemote;

// Use this subscribe to events e.g.
// `ocrCallbackRouter.onEventOccurred.addListener(handleEvent)`.
export const ocrCallbackRouter = new OcrUntrustedPageCallbackRouter();

// Used to create a connection to OcrUntrustedPageHandler.
const factoryRemote = UntrustedPageHandlerFactory.getRemote();

// Called when a new file that may require OCR is loaded. Closes the existing
// pipe and establishes a new one.
export function connectToOcrHandler() {
  if (ocrUntrustedPageHandler) {
    ocrUntrustedPageHandler.$.close();
  }
  ocrUntrustedPageHandler = new OcrUntrustedPageHandlerRemote();
  factoryRemote.createOcrUntrustedPageHandler(
      ocrUntrustedPageHandler.$.bindNewPipeAndPassReceiver(),
      ocrCallbackRouter.$.bindNewPipeAndPassRemote());
  return ocrUntrustedPageHandler;
}

// Used to make calls on the remote MahiUntrustedPageHandler interface.
// Singleton that client modules can use directly.
let mahiUntrustedPageHandler: MahiUntrustedPageHandlerRemote;

// Use this subscribe to Mahi concerned events e.g.
// `mahiCallbackRouter.eventOrRequest.addListener(handleEvent)`.
export const mahiCallbackRouter = new MahiUntrustedPageCallbackRouter();

// Called when a new PDF file that may support Mahi feature is loaded. Closes
// the existing pipe and establish a new one.
export function connectToMahiHandler(fileName?: string) {
  if (mahiUntrustedPageHandler) {
    mahiUntrustedPageHandler.$.close();
  }
  mahiUntrustedPageHandler = new MahiUntrustedPageHandlerRemote();
  factoryRemote.createMahiUntrustedPageHandler(
      mahiUntrustedPageHandler.$.bindNewPipeAndPassReceiver(),
      mahiCallbackRouter.$.bindNewPipeAndPassRemote(), fileName ?? '');
  return mahiUntrustedPageHandler;
}
