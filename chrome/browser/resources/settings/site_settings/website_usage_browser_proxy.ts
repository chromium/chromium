// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface WebsiteUsageBrowserProxy {
  fetchUsageTotal(host: string): void;
  clearUsage(origin: string): void;
}

export class WebsiteUsageBrowserProxyImpl implements WebsiteUsageBrowserProxy {
  fetchUsageTotal(host: string) {
    chrome.send('fetchUsageTotal', [host]);
  }

  clearUsage(origin: string) {
    chrome.send('clearUnpartitionedUsage', [origin]);
  }

  static getInstance(): WebsiteUsageBrowserProxy {
    return instance || (instance = new WebsiteUsageBrowserProxyImpl());
  }

  static setInstance(obj: WebsiteUsageBrowserProxy) {
    instance = obj;
  }
}

let instance: WebsiteUsageBrowserProxy|null = null;
