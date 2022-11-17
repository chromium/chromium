// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandlerFactory, PageHandlerRemote} from './cloud_upload.mojom-webui.js';

export abstract class CloudUploadBrowserProxy {
  handler: PageHandlerRemote;

  constructor() {
    this.handler = new PageHandlerRemote();
  }

  static getInstance(): CloudUploadBrowserProxy {
    return instance || (instance = new CloudUploadBrowserProxyImpl());
  }

  static setInstance(obj: CloudUploadBrowserProxy) {
    instance = obj;
  }

  // Whether or not this is a test proxy instance.
  isTest() {
    return false;
  }
}

class CloudUploadBrowserProxyImpl extends CloudUploadBrowserProxy {
  constructor() {
    super();
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(this.handler.$.bindNewPipeAndPassReceiver());
  }
}

let instance: CloudUploadBrowserProxy|null = null;
