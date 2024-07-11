// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxyBase} from './browser_proxy_base.js';

// Browser Proxy for the data sharing service.
// Only implement APIs related to data sharing service.
export class BrowserProxyApi extends BrowserProxyBase {
  static getInstance(): BrowserProxyApi {
    return instance || (instance = new BrowserProxyApi());
  }

  static setInstance(obj: BrowserProxyApi) {
    instance = obj;
  }
}

let instance: BrowserProxyApi|null = null;
