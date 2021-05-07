// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {StoreObserver} from 'chrome://resources/js/cr/ui/store.m.js';
import {StoreClient as CrUiStoreClient, StoreClientInterface as CrUiStoreClientInterface} from 'chrome://resources/js/cr/ui/store_client.m.js';

import {Store} from './store.js';
import {BookmarksPageState} from './types.js';

/**
 * @fileoverview Defines StoreClient, a Polymer behavior to tie a front-end
 * element to back-end data from the store.
 */

/**
 * @polymerBehavior
 */
const BookmarksStoreClientImpl = {
  /**
   * @param {string} localProperty
   * @param {function(Object)} valueGetter
   */
  watch(localProperty, valueGetter) {
    this.watch_(localProperty, valueGetter);
  },

  /**
   * @return {BookmarksPageState}
   */
  getState() {
    return this.getStore().data;
  },

  /**
   * @return {Store}
   */
  getStore() {
    return Store.getInstance();
  },
};

export class BookmarksStoreClientInterface {
  /**
   * @param {string} localProperty
   * @param {function(Object)} valueGetter
   */
  watch(localProperty, valueGetter) {}

  /** @return {BookmarksPageState} */
  getState() {}

  /** @return {Store} */
  getStore() {}
}

/**
 * @polymerBehavior
 * @implements {BookmarksStoreClientInterface}
 * @implements {CrUiStoreClientInterface}
 * @implements {StoreObserver<BookmarksPageState>}
 */
export const StoreClient = [CrUiStoreClient, BookmarksStoreClientImpl];
