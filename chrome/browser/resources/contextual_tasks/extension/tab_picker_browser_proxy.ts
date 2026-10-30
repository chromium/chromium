// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {MojoPluralStringProxy} from 'chrome://resources/cr_components/search/mojo_plural_string_proxy.js';
import {TabAttachmentSource} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageCallbackRouter as SearchboxPageCallbackRouter, TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {InputState} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';

export interface TabPickerBrowserProxy {
  getRecentTabs(): Promise<{tabs: TabInfo[]}>;
  getInputState(): Promise<{state: InputState|null}>;
  getSearchboxCallbackRouter(): SearchboxPageCallbackRouter;
  getPluralString(messageName: string, count: number): Promise<string>;
  addTabContext(tabId: number): void;
  deleteTabContext(tabId: number): void;
}

let instance: TabPickerBrowserProxy|null = null;

export class TabPickerBrowserProxyImpl implements TabPickerBrowserProxy {
  getRecentTabs(): Promise<{tabs: TabInfo[]}> {
    return ComposeboxProxyImpl.getInstance().searchboxHandler.getRecentTabs();
  }

  getInputState(): Promise<{state: InputState|null}> {
    return ComposeboxProxyImpl.getInstance().searchboxHandler.getInputState();
  }

  getSearchboxCallbackRouter(): SearchboxPageCallbackRouter {
    return ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
  }

  getPluralString(messageName: string, count: number): Promise<string> {
    return MojoPluralStringProxy.getInstance().getPluralString(
        messageName, count);
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
