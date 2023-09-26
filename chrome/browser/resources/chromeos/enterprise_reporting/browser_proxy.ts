// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerInterface, PageHandlerRemote} from './enterprise_reporting.mojom-webui.js';

export class EnterpriseReportingBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();

    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): EnterpriseReportingBrowserProxy {
    return instance || (instance = new EnterpriseReportingBrowserProxy());
  }

  static setInstance(obj: EnterpriseReportingBrowserProxy) {
    instance = obj;
  }
}

let instance: EnterpriseReportingBrowserProxy|null = null;
