// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './audio.mojom-webui.js';

export class AudioBroker {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerRemote;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();

    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  static getInstance() {
    return instance || (instance = new AudioBroker());
  }
}

let instance: AudioBroker|null = null;
