// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerInterface, PageHandlerRemote} from './enterprise_reporting.mojom-webui.js';

export class EnterpriseReportingBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;

  static getInstance(): EnterpriseReportingBrowserProxy {
    if (!instance) {
      const handler = new PageHandlerRemote();
      const callbackRouter = new PageCallbackRouter();
      PageHandlerFactory.getRemote().createPageHandler(
          callbackRouter.$.bindNewPipeAndPassRemote(),
          handler.$.bindNewPipeAndPassReceiver());
      instance = new EnterpriseReportingBrowserProxy(handler, callbackRouter);
    }
    return instance;
  }

  static createInstanceForTest(
      handler: PageHandlerInterface, callbackRouter: PageCallbackRouter) {
    instance = new EnterpriseReportingBrowserProxy(handler, callbackRouter);
  }

  private constructor(
      handler: PageHandlerInterface, callbackRouter: PageCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
  }
}

let instance: EnterpriseReportingBrowserProxy|null = null;
