// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

import {BookmarkPromoType} from './constants.js';
import type {IncognitoAvailability} from './constants.js';

// This is the data structure that is received from the browser.
export interface PromoCardData {
  promoType: BookmarkPromoType;
  canShow: boolean;
  promoTitle: string;
  promoSubtitle: string;
  actionButtonText: string;
  promoAvatarUrl?: string;
}

export interface BrowserProxy {
  getIncognitoAvailability(): Promise<IncognitoAvailability>;
  getCanEditBookmarks(): Promise<boolean>;
  getCanUploadBookmarkToAccountStorage(id: string): Promise<boolean>;
  recordInHistogram(histogram: string, bucket: number, maxBucket: number): void;
  onSingleBookmarkUploadClicked(bookmarkId: string): void;

  // Promo card functions
  getPromoData(): Promise<PromoCardData>;
  onPromoShown(): void;
  onPromoClicked(): void;
  onPromoDismissed(): void;
}

export class BrowserProxyImpl implements BrowserProxy {
  getIncognitoAvailability() {
    return sendWithPromise<IncognitoAvailability>('getIncognitoAvailability');
  }

  getCanEditBookmarks() {
    return sendWithPromise<boolean>('getCanEditBookmarks');
  }

  getCanUploadBookmarkToAccountStorage(id: string) {
    // <if expr="enable_bookmarks_single_upload">
    return sendWithPromise<boolean>('getCanUploadBookmarkToAccountStorage', id);
    // </if>
    // <if expr="not enable_bookmarks_single_upload">
    // Per-item account upload is only supported on non-Android platforms.
    void id;
    return Promise.resolve(false);
    // </if>
  }

  recordInHistogram(histogram: string, bucket: number, maxBucket: number) {
    chrome.send(
        'metricsHandler:recordInHistogram', [histogram, bucket, maxBucket]);
  }

  onSingleBookmarkUploadClicked(bookmarkId: string) {
    chrome.send('onSingleBookmarkUploadClicked', [bookmarkId]);
  }

  getPromoData(): Promise<PromoCardData> {
    // <if expr="enable_bookmarks_promo">
    return sendWithPromise<PromoCardData>('getPromoData');
    // </if>
    // <if expr="not enable_bookmarks_promo">
    // TODO(b/587475184): Support <promo-card> promos on Android.
    return Promise.resolve({
      promoType: BookmarkPromoType.NONE,
      canShow: false,
      promoTitle: '',
      promoSubtitle: '',
      actionButtonText: '',
    });
    // </if>
  }

  onPromoShown(): void {
    chrome.send('onPromoShown');
  }

  onPromoClicked(): void {
    chrome.send('onPromoClicked');
  }

  onPromoDismissed(): void {
    chrome.send('onPromoDismissed');
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
