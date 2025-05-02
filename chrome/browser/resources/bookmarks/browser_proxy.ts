// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

import type {IncognitoAvailability} from './constants.js';

// This is the data structure that is received from the browser.
export interface BatchUploadPromoData {
  canShow: boolean;
  localBookmarksCount: number;
  email: string;
}

export interface BrowserProxy {
  getIncognitoAvailability(): Promise<IncognitoAvailability>;
  getCanEditBookmarks(): Promise<boolean>;
  getCanUploadBookmarkToAccountStorage(id: string): Promise<boolean>;
  recordInHistogram(histogram: string, bucket: number, maxBucket: number): void;

  // Promo/BatchUpload functions
  // TODO(crbug.com/411439975): Consider using it's own handler, with a mojo
  // implementation.
  getBatchUploadPromoInfo(): Promise<BatchUploadPromoData>;
  onBatchUploadPromoClicked(): void;
  onBatchUploadPromoDismissed(): void;
}

export class BrowserProxyImpl implements BrowserProxy {
  getIncognitoAvailability() {
    return sendWithPromise('getIncognitoAvailability');
  }

  getCanEditBookmarks() {
    return sendWithPromise('getCanEditBookmarks');
  }

  getCanUploadBookmarkToAccountStorage(id: string) {
    return sendWithPromise('getCanUploadBookmarkToAccountStorage', id);
  }

  recordInHistogram(histogram: string, bucket: number, maxBucket: number) {
    chrome.send(
        'metricsHandler:recordInHistogram', [histogram, bucket, maxBucket]);
  }

  getBatchUploadPromoInfo() {
    // TODO(crbug.com/411439975): redirect to proxy to retrieve the right data.
    return Promise.resolve({
      canShow: false,
      localBookmarksCount: 0,
      email: 'test@gmail.com',
    });
  }

  onBatchUploadPromoClicked(): void {
    // TODO(crbug.com/411439975): redirect to proxy to open batch upload.
  }

  onBatchUploadPromoDismissed(): void {
    // TODO(crbug.com/411439975): redirect to proxy to update dismiss count.
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
