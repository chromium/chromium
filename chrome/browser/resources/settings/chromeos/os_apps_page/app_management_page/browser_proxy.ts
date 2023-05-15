// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerInterface} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy as AppManagementComponentBrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';

// Export this module instance that is bundled locally.
export {AppManagementComponentBrowserProxy};

let instance: AppManagementBrowserProxy|null = null;

export class AppManagementBrowserProxy {
  static getInstance(): AppManagementBrowserProxy {
    return instance || (instance = new AppManagementBrowserProxy());
  }

  static setInstanceForTesting(obj: AppManagementBrowserProxy): void {
    instance = obj;
  }

  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;

  constructor() {
    this.handler = AppManagementComponentBrowserProxy.getInstance().handler;
    this.callbackRouter =
        AppManagementComponentBrowserProxy.getInstance().callbackRouter;
  }
}
