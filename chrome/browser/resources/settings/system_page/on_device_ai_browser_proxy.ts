// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface OnDeviceAiEnabled {
  enabled: boolean;
  allowedByPolicy: boolean;
}

export interface OnDeviceAiBrowserProxy {
  getOnDeviceAiEnabled(): Promise<OnDeviceAiEnabled>;
  setOnDeviceAiEnabled(enabled: boolean): void;
}

export class OnDeviceAiBrowserProxyImpl implements OnDeviceAiBrowserProxy {
  getOnDeviceAiEnabled(): Promise<OnDeviceAiEnabled> {
    return sendWithPromise('getOnDeviceAiEnabled');
  }

  setOnDeviceAiEnabled(enabled: boolean): void {
    chrome.send('setOnDeviceAiEnabled', [enabled]);
  }

  static getInstance(): OnDeviceAiBrowserProxy {
    return instance || (instance = new OnDeviceAiBrowserProxyImpl());
  }

  static setInstance(obj: OnDeviceAiBrowserProxy) {
    instance = obj;
  }
}

let instance: OnDeviceAiBrowserProxy|null = null;
