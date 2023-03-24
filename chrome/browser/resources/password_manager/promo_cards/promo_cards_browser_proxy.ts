// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface PromoCard {
  id: string;
  title: string;
  description: string;
  actionButtonText?: string;
}

export interface PromoCardsProxy {
  /**
   * Returns promo card to show, or null if there are no available promo cards.
   */
  getAvailablePromoCard(): Promise<PromoCard|null>;

  /**
   * Records dismissal of a promo card. This is important to determine whether
   * promo should be shown in the future.
   */
  recordPromoDismissed(id: string): void;
}

export class PromoCardsProxyImpl implements PromoCardsProxy {
  getAvailablePromoCard() {
    return sendWithPromise('getAvailablePromoCard');
  }

  recordPromoDismissed(id: string) {
    chrome.send('recordPromoDismissed', [id]);
  }

  static getInstance(): PromoCardsProxy {
    return instance || (instance = new PromoCardsProxyImpl());
  }

  static setInstance(obj: PromoCardsProxy) {
    instance = obj;
  }
}

let instance: PromoCardsProxy|null = null;
