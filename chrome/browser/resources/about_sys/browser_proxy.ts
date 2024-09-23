// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface SystemLog {
  statName: string;
  statValue: string;
}

export interface BrowserProxy {
  requestSystemInfo(): Promise<SystemLog[]>;

  // <if expr="chromeos_ash">
  isLacrosEnabled(): Promise<boolean>;
  openLacrosSystemPage(): void;
  // </if>
}

export class BrowserProxyImpl implements BrowserProxy {
  requestSystemInfo() {
    return sendWithPromise('requestSystemInfo');
  }

  // <if expr="chromeos_ash">
  isLacrosEnabled() {
    return sendWithPromise('isLacrosEnabled');
  }

  openLacrosSystemPage() {
    chrome.send('openLacrosSystemPage');
  }
  // </if>

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
