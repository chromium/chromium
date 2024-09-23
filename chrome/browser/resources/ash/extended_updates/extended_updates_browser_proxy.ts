// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {App, PageHandlerInterface} from './extended_updates.mojom-webui.js';
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './extended_updates.mojom-webui.js';

export class ExtendedUpdatesBrowserProxy {
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

  static getInstance(): ExtendedUpdatesBrowserProxy {
    return instance || (instance = new ExtendedUpdatesBrowserProxy());
  }

  static setInstance(obj: ExtendedUpdatesBrowserProxy) {
    instance = obj;
  }

  async optInToExtendedUpdates(): Promise<boolean> {
    const {success} = await this.handler.optInToExtendedUpdates();
    return success;
  }

  closeDialog(): void {
    this.handler.closeDialog();
  }

  async getInstalledAndroidApps(): Promise<App[]> {
    const {apps} = await this.handler.getInstalledAndroidApps();
    return apps;
  }
}

let instance: ExtendedUpdatesBrowserProxy|null = null;
