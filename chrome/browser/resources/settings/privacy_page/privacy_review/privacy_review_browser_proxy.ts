// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

// The number of times the prviacy review promo has been shown.
export const MAX_PRIVACY_REVIEW_PROMO_IMPRESSION: number = 10;

// Key to be used with the localStorage for the privacy review promo.
const PRIVACY_REVIEW_PROMO_IMPRESSION_COUNT_KEY: string =
    'privacy-review-promo-count';

export interface PrivacyReviewBrowserProxy {
  /** Get the current privacy review status. */
  isPrivacyReviewAvailable(): Promise<boolean>;

  /** @return The number of times the privacy review promo was shown. */
  getPromoImpressionCount(): number;

  /** Increment The number of times the privacy review promo was shown. */
  incrementPromoImpressionCount(): void;
}

export class PrivacyReviewBrowserProxyImpl implements
    PrivacyReviewBrowserProxy {
  isPrivacyReviewAvailable() {
    return sendWithPromise('isPrivacyReviewAvailable');
  }

  getPromoImpressionCount() {
    return parseInt(
               window.localStorage.getItem(
                   PRIVACY_REVIEW_PROMO_IMPRESSION_COUNT_KEY)!,
               10) ||
        0;
  }

  incrementPromoImpressionCount() {
    window.localStorage.setItem(
        PRIVACY_REVIEW_PROMO_IMPRESSION_COUNT_KEY,
        (this.getPromoImpressionCount() + 1).toString());
  }

  static getInstance(): PrivacyReviewBrowserProxy {
    return instance || (instance = new PrivacyReviewBrowserProxyImpl());
  }

  static setInstance(obj: PrivacyReviewBrowserProxy) {
    instance = obj;
  }
}

let instance: PrivacyReviewBrowserProxy|null = null;
