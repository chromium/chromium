// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxyImpl} from './browser_proxy.js';

const browserProxy = BrowserProxyImpl.getInstance();

// Demo to verify the handler is connected.
// TODO(harringtond): Replace this with actual functionality.
async function initialize() {
  const response = await browserProxy.handler.getChromeVersion();
  document.body.append(
      `Version: ${JSON.stringify(response.version.components)}`);
}

document.addEventListener('DOMContentLoaded', initialize);
