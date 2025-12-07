// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {NtpPromoHandlerInterface} from '../ntp_promo.mojom-webui.js';
import {NtpPromoClientCallbackRouter, NtpPromoHandlerFactory, NtpPromoHandlerRemote} from '../ntp_promo.mojom-webui.js';

export interface NtpPromoProxy {
  getHandler(): NtpPromoHandlerInterface;
  getCallbackRouter(): NtpPromoClientCallbackRouter;
}

export class NtpPromoProxyImpl implements NtpPromoProxy {
  private callbackRouter_ = new NtpPromoClientCallbackRouter();
  private handler_ = new NtpPromoHandlerRemote();

  constructor() {
    const factory = NtpPromoHandlerFactory.getRemote();
    factory.createNtpPromoHandler(
        this.callbackRouter_.$.bindNewPipeAndPassRemote(),
        this.handler_.$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): NtpPromoProxy {
    return instance || (instance = new NtpPromoProxyImpl());
  }

  static setInstance(obj: NtpPromoProxy) {
    instance = obj;
  }

  getHandler(): NtpPromoHandlerRemote {
    return this.handler_;
  }

  getCallbackRouter(): NtpPromoClientCallbackRouter {
    return this.callbackRouter_;
  }
}

let instance: NtpPromoProxy|null = null;
