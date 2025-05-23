// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Web Store Discovery" dialog to
 * interact with the browser.
 */
import {PageHandlerFactory, PageHandlerRemote} from './zero_state_promo.mojom-webui.js';
import type {WebStoreLinkClicked} from './zero_state_promo.mojom-webui.js';

let instance: ZeroStatePromoBrowserProxy|null = null;

export interface ZeroStatePromoBrowserProxy {
  launchWebStoreLink(link: WebStoreLinkClicked): void;
}

export class ZeroStatePromoBrowserProxyImpl implements
    ZeroStatePromoBrowserProxy {
  private handler_: PageHandlerRemote;

  constructor() {
    this.handler_ = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(this.handler_.$.bindNewPipeAndPassReceiver());
  }

  launchWebStoreLink(link: WebStoreLinkClicked) {
    this.handler_.launchWebStoreLink(link);
  }

  static getInstance(): ZeroStatePromoBrowserProxy {
    return instance || (instance = new ZeroStatePromoBrowserProxyImpl());
  }

  static setInstance(obj: ZeroStatePromoBrowserProxy) {
    instance = obj;
  }
}
