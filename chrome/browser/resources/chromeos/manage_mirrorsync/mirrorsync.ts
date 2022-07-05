// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from './browser_proxy.js';

async function initialize() {
  const proxy = BrowserProxy.getInstance().handler;
  /* eslint-disable no-console */
  console.log(await proxy.getSyncingPaths());
}

document.addEventListener('DOMContentLoaded', initialize);