// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FeedSidePanelCallbackRouter, FeedSidePanelHandlerFactory, FeedSidePanelHandlerRemote} from './feed.mojom-webui.js';

// Used to make calls on the remote PageHandler interface. Singleton that client
// modules can use directly.
export const feedSidePanelHandler = new FeedSidePanelHandlerRemote();
export const feedSidePanelCallbackRouter = new FeedSidePanelCallbackRouter();
// Use PageHandlerFactory to create a connection to PageHandler.
const factoryRemote = FeedSidePanelHandlerFactory.getRemote();
factoryRemote.createFeedSidePanelHandler(
    feedSidePanelHandler.$.bindNewPipeAndPassReceiver(),
    feedSidePanelCallbackRouter.$.bindNewPipeAndPassRemote());