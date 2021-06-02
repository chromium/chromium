// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';

import {IncognitoAvailability} from './constants.js';

export class BrowserProxy {
  getIncognitoAvailability(): Promise<IncognitoAvailability> {
    return sendWithPromise('getIncognitoAvailability');
  }

  getCanEditBookmarks(): Promise<boolean> {
    return sendWithPromise('getCanEditBookmarks');
  }

  recordInHistogram(histogram: string, bucket: number, maxBucket: number) {
    chrome.send(
        'metricsHandler:recordInHistogram', [histogram, bucket, maxBucket]);
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxy());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
