// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxyBase} from './browser_proxy_base.js';

// Browser proxy for data sharing UI.
// Only implement APIs related to data sharing UI.
export class BrowserProxy extends BrowserProxyBase {
  constructor() {
    super();
    this.handler.showUI();
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxy());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
