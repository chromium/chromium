// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BocaPageHandlerFactory, PageCallbackRouter, PageHandlerRemote} from '../mojom/boca.mojom-webui.js';

// Used to make calls on the remote PageHandler interface. Singleton that client
// modules can use directly.
export const pageHandler = new PageHandlerRemote();

// Use this subscribe to events e.g.
// `callbackRouter.onEventOccurred.addListener(handleEvent)`.
export const callbackRouter = new PageCallbackRouter();

// Use PageHandlerFactory to create a connection to PageHandler.
const factoryRemote = BocaPageHandlerFactory.getRemote();
factoryRemote.create(
    pageHandler.$.bindNewPipeAndPassReceiver(),
    callbackRouter.$.bindNewPipeAndPassRemote());
