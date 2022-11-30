// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './segmentation_internals.mojom-webui.js';

export class SegmentationInternalsBrowserProxy {
  private handler: PageHandlerRemote;
  private callbackRouter: PageCallbackRouter;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new PageHandlerRemote();
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  getServiceStatus() {
    return this.handler.getServiceStatus();
  }

  executeModel(target: number) {
    return this.handler.executeModel(target);
  }

  overwriteResult(target: number, result: number) {
    return this.handler.overwriteResult(target, result);
  }

  setSelected(segmentationKey: string, target: number) {
    return this.handler.setSelected(segmentationKey, target);
  }

  static getInstance(): SegmentationInternalsBrowserProxy {
    return instance || (instance = new SegmentationInternalsBrowserProxy());
  }

  getCallbackRouter(): PageCallbackRouter {
    return this.callbackRouter;
  }
}

let instance: SegmentationInternalsBrowserProxy|null = null;