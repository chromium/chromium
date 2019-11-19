// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {StoreClient as CrUiStoreClient} from 'chrome://resources/js/cr/ui/store_client.m.js';
import {StoreObserver} from 'chrome://resources/js/cr/ui/store.m.js';
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
    watch: function(localProperty, valueGetter) {
      this.watch_(localProperty, valueGetter);
    },

    /**
     * @return {BookmarksPageState}
     */
    getState: function() {
      return this.getStore().data;
    },

    /**
     * @return {Store}
     */
    getStore: function() {
      return Store.getInstance();
    },
  };

  /**
   * @polymerBehavior
   * @implements {StoreObserver<BookmarksPageState>}
   */
  export const StoreClient = [
    CrUiStoreClient,
    BookmarksStoreClientImpl
  ];

