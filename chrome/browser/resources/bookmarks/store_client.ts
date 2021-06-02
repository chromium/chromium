// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {StoreObserver} from 'chrome://resources/js/cr/ui/store.m.js';
import {StoreClient as CrUiStoreClient, StoreClientInterface as CrUiStoreClientInterface} from 'chrome://resources/js/cr/ui/store_client.m.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Store} from './store.js';
import {BookmarksPageState} from './types.js';

/**
 * @fileoverview Defines StoreClient, a Polymer behavior to tie a front-end
 * element to back-end data from the store.
 */

const BookmarksStoreClientImpl = {
  watch(localProperty: string, valueGetter: (p: BookmarksPageState) => any) {
    (this as any).watch_(localProperty, valueGetter);
  },

  getState(): BookmarksPageState {
    return this.getStore().data;
  },

  getStore(): Store {
    return Store.getInstance();
  },
};

export interface BookmarksStoreClientInterface extends
    CrUiStoreClientInterface {
  watch(localProperty: string,
        valueGetter: (p: BookmarksPageState) => any): void;

  getState(): BookmarksPageState;

  getStore(): Store;
}

export const StoreClient = [CrUiStoreClient, BookmarksStoreClientImpl];
