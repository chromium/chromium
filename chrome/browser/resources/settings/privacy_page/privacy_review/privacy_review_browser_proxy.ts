// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

export interface PrivacyReviewBrowserProxy {
  /** Get the current privacy review status. */
  isPrivacyReviewAvailable(): Promise<boolean>;
}

export class PrivacyReviewBrowserProxyImpl implements
    PrivacyReviewBrowserProxy {
  isPrivacyReviewAvailable() {
    return sendWithPromise('isPrivacyReviewAvailable');
  }

  static getInstance(): PrivacyReviewBrowserProxy {
    return instance || (instance = new PrivacyReviewBrowserProxyImpl());
  }

  static setInstance(obj: PrivacyReviewBrowserProxy) {
    instance = obj;
  }
}

let instance: PrivacyReviewBrowserProxy|null = null;
