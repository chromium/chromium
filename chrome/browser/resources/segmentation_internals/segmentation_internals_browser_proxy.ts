// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandlerFactory, PageHandlerRemote} from './segmentation_internals.mojom-webui.js';

export class SegmentationInternalsBrowserProxy {
  private handler: PageHandlerRemote;

  constructor() {
    this.handler = new PageHandlerRemote();
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(this.handler.$.bindNewPipeAndPassReceiver());
  }

  getSegment(key: string) {
    return this.handler.getSegment(key);
  }

  static getInstance(): SegmentationInternalsBrowserProxy {
    return instance || (instance = new SegmentationInternalsBrowserProxy());
  }
}

let instance: SegmentationInternalsBrowserProxy|null = null;
