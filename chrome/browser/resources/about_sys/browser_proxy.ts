// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface SystemLog {
  statName: string;
  statValue: string;
}

interface BrowserProxy {
  requestSystemInfo(): Promise<SystemLog[]>;

  isLacrosEnabled(): Promise<boolean>;

  openLacrosSystemPage(): void;
}

export class BrowserProxyImpl implements BrowserProxy {
  requestSystemInfo() {
    return sendWithPromise('requestSystemInfo');
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }

  isLacrosEnabled() {
    return sendWithPromise('isLacrosEnabled');
  }

  openLacrosSystemPage() {
    chrome.send('openLacrosSystemPage');
  }
}

let instance: BrowserProxy|null = null;
