// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

// WARNING: Keep synced with
// chrome/browser/ui/webui/password_manager/sync_handler.cc.
export enum TrustedVaultBannerState {
  NOT_SHOWN = 0,
  OFFER_OPT_IN = 1,
  OPTED_IN = 2,
}

export interface SyncBrowserProxy {
  /**
   * Gets trusted vault banner state.
   */
  getTrustedVaultBannerState(): Promise<TrustedVaultBannerState>;
}

export class SyncBrowserProxyImpl implements SyncBrowserProxy {
  getTrustedVaultBannerState() {
    return sendWithPromise('GetSyncTrustedVaultBannerState');
  }

  static getInstance(): SyncBrowserProxy {
    return instance || (instance = new SyncBrowserProxyImpl());
  }

  static setInstance(obj: SyncBrowserProxy) {
    instance = obj;
  }
}

let instance: SyncBrowserProxy|null = null;
