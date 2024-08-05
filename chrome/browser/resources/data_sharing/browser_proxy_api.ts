// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxyBase} from './browser_proxy_base.js';
import type {GroupData} from './group_data.mojom-webui.js';

// Browser Proxy for the data sharing service.
// Only implement APIs related to data sharing service.
export class BrowserProxyApi extends BrowserProxyBase {
  constructor() {
    super();

    this.callbackRouter.readGroups.addListener((groupIds: string[]) => {
      // Dummy implementation of readGroups API.
      // TODO(b/346625367): Replace this with real implementation.
      return new Promise((resolve) => {
        setTimeout(() => {
          const groups: GroupData[] = [];
          for (const groupId of groupIds) {
            groups.push({
              groupId: groupId,
              displayName: 'test',
              accessToken: 'abc',
              members: [],
            });
          }
          resolve({groups});
        }, 1);
      });
    });
  }

  static getInstance(): BrowserProxyApi {
    return instance || (instance = new BrowserProxyApi());
  }

  static setInstance(obj: BrowserProxyApi) {
    instance = obj;
  }
}

let instance: BrowserProxyApi|null = null;
