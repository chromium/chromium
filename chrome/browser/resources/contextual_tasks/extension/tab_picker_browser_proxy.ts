// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {TabAttachmentSource} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

export interface TabPickerBrowserProxy {
  getRecentTabs(): Promise<{tabs: TabInfo[]}>;
  getPluralString(messageName: string, count: number): Promise<string>;
  addTabContext(tabId: number): void;
  deleteTabContext(tabId: number): void;
}

let instance: TabPickerBrowserProxy|null = null;

export class TabPickerBrowserProxyImpl implements TabPickerBrowserProxy {
  getRecentTabs(): Promise<{tabs: TabInfo[]}> {
    return ComposeboxProxyImpl.getInstance().searchboxHandler.getRecentTabs();
  }

  getPluralString(messageName: string, count: number): Promise<string> {
    if (messageName === 'sharingTabs') {
      return Promise.resolve(
          count === 1 ? 'Sharing 1 tab' : `Sharing ${count} tabs`);
    }
    return Promise.resolve('');
  }

  addTabContext(tabId: number): void {
    ComposeboxProxyImpl.getInstance()
        .searchboxHandler
        .addTabContext(
            tabId, /*delayUpload=*/ false, TabAttachmentSource.kContextMenu)
        .catch(() => {});
  }

  deleteTabContext(tabId: number): void {
    ComposeboxProxyImpl.getInstance().searchboxHandler.deleteTabContext(tabId);
  }

  static getInstance(): TabPickerBrowserProxy {
    return instance || (instance = new TabPickerBrowserProxyImpl());
  }

  static setInstance(newInstance: TabPickerBrowserProxy) {
    instance = newInstance;
  }
}
