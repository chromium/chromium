// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OcrUntrustedPageCallbackRouter, UntrustedPageHandlerFactory, OcrUntrustedPageHandlerRemote} from './media_app_ui_untrusted.mojom-webui.js';

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
