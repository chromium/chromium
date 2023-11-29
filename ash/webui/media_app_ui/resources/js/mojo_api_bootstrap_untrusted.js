// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './geometry.mojom-lite.js';
import './media_app_ui_untrusted.mojom-lite.js';

// Used to make calls on the remote OcrUntrustedPageHandler interface. Singleton
// that client modules can use directly.
export const ocrUntrustedPageHandler =
    new ash.mediaAppUi.mojom.OcrUntrustedPageHandlerRemote();

// Use this subscribe to events e.g.
// `ocrCallbackRouter.onEventOccurred.addListener(handleEvent)`.
export const ocrCallbackRouter =
    new ash.mediaAppUi.mojom.OcrUntrustedPageCallbackRouter();

// Use UntrustedPageHandlerFactory to create a connection to
// OcrUntrustedPageHandler.
const factoryRemote =
    ash.mediaAppUi.mojom.UntrustedPageHandlerFactory.getRemote();
factoryRemote.createOcrUntrustedPageHandler(
    ocrUntrustedPageHandler.$.bindNewPipeAndPassReceiver(),
    ocrCallbackRouter.$.bindNewPipeAndPassRemote());
