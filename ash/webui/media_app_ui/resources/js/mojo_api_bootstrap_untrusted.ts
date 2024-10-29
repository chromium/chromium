// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MahiUntrustedPageCallbackRouter, MahiUntrustedPageHandlerRemote, MantisMediaAppUntrustedServiceRemote, OcrUntrustedPageCallbackRouter, OcrUntrustedServiceRemote, UntrustedPageHandlerFactory} from './media_app_ui_untrusted.mojom-webui.js';

// Used to make calls on the remote OcrUntrustedService interface. Singleton
// that client modules can use directly.
let ocrUntrustedService: OcrUntrustedServiceRemote;

// Use this subscribe to events e.g.
// `ocrCallbackRouter.onEventOccurred.addListener(handleEvent)`.
export const ocrCallbackRouter = new OcrUntrustedPageCallbackRouter();

// Used to create a connection to the services used in the untrusted context
// (Mahi, OCR, and Mantis).
const factoryRemote = UntrustedPageHandlerFactory.getRemote();

// Called when a new file that may require OCR is loaded. Closes the existing
// pipe and establishes a new one.
export function connectToOcrUntrustedService() {
  if (ocrUntrustedService) {
    ocrUntrustedService.$.close();
  }
  ocrUntrustedService = new OcrUntrustedServiceRemote();
  factoryRemote.createOcrUntrustedService(
      ocrUntrustedService.$.bindNewPipeAndPassReceiver(),
      ocrCallbackRouter.$.bindNewPipeAndPassRemote());
  return ocrUntrustedService;
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

let mantisUntrustedService: MantisMediaAppUntrustedServiceRemote;

export function connectToMantisUntrustedService() {
  if (mantisUntrustedService) {
    mantisUntrustedService.$.close();
  }
  mantisUntrustedService = new MantisMediaAppUntrustedServiceRemote();
  factoryRemote.createMantisUntrustedService(
      mantisUntrustedService.$.bindNewPipeAndPassReceiver());
  return mantisUntrustedService;
}
