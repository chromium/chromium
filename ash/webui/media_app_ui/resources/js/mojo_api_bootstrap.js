// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './media_app_ui.mojom-lite.js';

export const mediaAppPageHandler = new ash.mediaAppUi.mojom.PageHandlerRemote();

// Set up a page handler to talk to the browser process.
ash.mediaAppUi.mojom.PageHandlerFactory.getRemote().createPageHandler(
    mediaAppPageHandler.$.bindNewPipeAndPassReceiver());
