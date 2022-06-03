// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Store} from 'chrome://resources/js/cr/ui/store.js';
import {StoreClient} from 'chrome://resources/js/cr/ui/store_client.js';

import {AppManagementStore} from './store.js';

/**
 * @fileoverview Defines StoreClient, a Polymer behavior to tie a front-end
 * element to back-end data from the store.
 */

/**
 * @polymerBehavior
 */
export const AppManagementStoreClientImpl = {
  /**
   * @param {string} localProperty
   * @param {function(!AppManagementPageState)} valueGetter
   */
  watch(localProperty, valueGetter) {
    this.watch_(localProperty, /** @type {function(!Object)} */ (valueGetter));
  },

  /**
   * @return {AppManagementPageState}
   */
  getState() {
    return this.getStore().data;
  },

  /**
   * @return {Store<AppManagementPageState>}
   */
  getStore() {
    return AppManagementStore.getInstance();
  },
};

/**
 * @polymerBehavior
 * @implements {StoreObserver}
 */
export const AppManagementStoreClient =
    [StoreClient, AppManagementStoreClientImpl];
