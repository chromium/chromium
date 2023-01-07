// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The number of times the prviacy guide promo has been shown.
export const MAX_PRIVACY_GUIDE_PROMO_IMPRESSION: number = 10;

// Key to be used with the localStorage for the privacy guide promo.
const PRIVACY_GUIDE_PROMO_IMPRESSION_COUNT_KEY: string =
    'privacy-guide-promo-count';

export interface PrivacyGuideBrowserProxy {
  /** @return The number of times the privacy guide promo was shown. */
  getPromoImpressionCount(): number;

  /** Increment The number of times the privacy guide promo was shown. */
  incrementPromoImpressionCount(): void;
}

export class PrivacyGuideBrowserProxyImpl implements PrivacyGuideBrowserProxy {
  getPromoImpressionCount() {
    return parseInt(
               window.localStorage.getItem(
                   PRIVACY_GUIDE_PROMO_IMPRESSION_COUNT_KEY)!,
               10) ||
        0;
  }

  incrementPromoImpressionCount() {
    window.localStorage.setItem(
        PRIVACY_GUIDE_PROMO_IMPRESSION_COUNT_KEY,
        (this.getPromoImpressionCount() + 1).toString());
  }

  static getInstance(): PrivacyGuideBrowserProxy {
    return instance || (instance = new PrivacyGuideBrowserProxyImpl());
  }

  static setInstance(obj: PrivacyGuideBrowserProxy) {
    instance = obj;
  }
}

let instance: PrivacyGuideBrowserProxy|null = null;
