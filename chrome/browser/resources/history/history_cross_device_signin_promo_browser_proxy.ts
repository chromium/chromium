// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {HistoryCrossDeviceSigninPromoHandler} from 'chrome://resources/cr_components/history/history_cross_device_signin_promo.mojom-webui.js';
import type {HistoryCrossDeviceSigninPromoHandlerRemote} from 'chrome://resources/cr_components/history/history_cross_device_signin_promo.mojom-webui.js';

export class HistoryCrossDeviceSigninPromoBrowserProxy {
  handler: HistoryCrossDeviceSigninPromoHandlerRemote;

  constructor(handler: HistoryCrossDeviceSigninPromoHandlerRemote) {
    this.handler = handler;
  }

  static getInstance(): HistoryCrossDeviceSigninPromoBrowserProxy {
    if (!instance) {
      instance = new HistoryCrossDeviceSigninPromoBrowserProxy(
          HistoryCrossDeviceSigninPromoHandler.getRemote());
    }
    return instance;
  }

  static setInstance(obj: HistoryCrossDeviceSigninPromoBrowserProxy) {
    instance = obj;
  }
}

let instance: HistoryCrossDeviceSigninPromoBrowserProxy|null = null;
