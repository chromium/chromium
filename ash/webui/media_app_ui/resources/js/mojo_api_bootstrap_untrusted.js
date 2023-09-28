// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './media_app_ui_untrusted.mojom-lite.js';

// Used to make calls on the remote UntrustedPageHandler interface. Singleton
// that client modules can use directly.
export const untrustedPageHandler =
    new ash.mediaAppUi.mojom.UntrustedPageHandlerRemote();

// Use this subscribe to events e.g.
// `callbackRouter.onEventOccurred.addListener(handleEvent)`.
export const callbackRouter =
    new ash.mediaAppUi.mojom.UntrustedPageHandlerCallbackRouter();

// Use UntrustedPageHandlerFactory to create a connection to
// UntrustedPageHandler.
const factoryRemote =
    ash.mediaAppUi.mojom.UntrustedPageHandlerFactory.getRemote();
factoryRemote.createUntrustedPageHandler(
    untrustedPageHandler.$.bindNewPipeAndPassReceiver(),
    callbackRouter.$.bindNewPipeAndPassRemote());
