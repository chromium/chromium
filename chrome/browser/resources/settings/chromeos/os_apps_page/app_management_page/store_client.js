// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Store} from 'chrome://resources/ash/common/store/store.js';
import {StoreClient, StoreClientInterface} from 'chrome://resources/ash/common/store/store_client.js';

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
   * @override
   * @return {!AppManagementPageState}
   */
  getState() {
    return this.getStore().data;
  },

  /**
   * @override
   * @return {!Store<AppManagementPageState>}
   */
  getStore() {
    return AppManagementStore.getInstance();
  },
};

/**
 * @interface
 * @extends {StoreClientInterface<AppManagementPageState>}
 */
export class AppManagementStoreClientInterface {
  /**
   * @param {string} localProperty
   * @param {function(!AppManagementPageState)} valueGetter
   */
  watch(localProperty, valueGetter) {}

  /**
   * @override
   * @return {!AppManagementPageState}
   */
  getState() {}

  /**
   * @override
   * @return {!Store<AppManagementPageState>}
   */
  getStore() {}
}

/**
 * @polymerBehavior
 * @implements {StoreObserver}
 */
export const AppManagementStoreClient =
    [StoreClient, AppManagementStoreClientImpl];
