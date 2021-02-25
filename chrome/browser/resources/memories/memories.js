// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MemoriesResult} from '/chrome/browser/ui/webui/memories/memories.mojom-webui.js';

import {BrowserProxy} from './browser_proxy.js';

// <if expr="not is_official_build">
const query = decodeURI(window.location.hash.substr(1));
if (query) {
  BrowserProxy.getInstance().handler.getSampleMemories(query).then(
      (/** @type {!MemoriesResult} */ result) => {
        console.log(result);
      });
}
// </if>
