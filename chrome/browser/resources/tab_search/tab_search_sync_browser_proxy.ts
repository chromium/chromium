// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * @see chrome/browser/ui/webui/tab_search/tab_search_sync_handler.cc
 */
export interface AccountInfo {
  name: string;
  email: string;
  avatarImage?: string;
}

export interface SyncInfo {
  syncing: boolean;
  paused: boolean;
  syncingHistory: boolean;
}

export interface TabSearchSyncBrowserProxy {
  /**
   * Gets the current sync info.
   */
  getSyncInfo(): Promise<SyncInfo>;

  /**
   * Gets the current account info.
   */
  getAccountInfo(): Promise<AccountInfo>;
}

export class TabSearchSyncBrowserProxyImpl implements
    TabSearchSyncBrowserProxy {
  getSyncInfo() {
    return sendWithPromise('GetSyncInfo');
  }

  getAccountInfo() {
    return sendWithPromise('GetAccountInfo');
  }

  static getInstance(): TabSearchSyncBrowserProxy {
    return instance || (instance = new TabSearchSyncBrowserProxyImpl());
  }

  static setInstance(obj: TabSearchSyncBrowserProxy) {
    instance = obj;
  }
}

let instance: TabSearchSyncBrowserProxy|null = null;
