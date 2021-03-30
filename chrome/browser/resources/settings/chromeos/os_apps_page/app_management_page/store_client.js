// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {StoreClient} from 'chrome://resources/js/cr/ui/store_client.m.js';
// #import {Store} from 'chrome://resources/js/cr/ui/store.m.js';
// #import {AppManagementStore} from './store.m.js';
// clang-format on

/**
 * @fileoverview Defines StoreClient, a Polymer behavior to tie a front-end
 * element to back-end data from the store.
 */

cr.define('app_management', function() {
  /**
   * @polymerBehavior
   */
  /* #export */ const AppManagementStoreClientImpl = {
    /**
     * @param {string} localProperty
     * @param {function(!AppManagementPageState)} valueGetter
     */
    watch(localProperty, valueGetter) {
      this.watch_(
          localProperty, /** @type {function(!Object)} */ (valueGetter));
    },

    /**
     * @return {AppManagementPageState}
     */
    getState() {
      return this.getStore().data;
    },

    /**
     * @return {cr.ui.Store<AppManagementPageState>}
     */
    getStore() {
      return app_management.AppManagementStore.getInstance();
    },
  };

  /**
   * @polymerBehavior
   * @implements {cr.ui.StoreObserver}
   */
  /* #export */ const AppManagementStoreClient =
      [cr.ui.StoreClient, AppManagementStoreClientImpl];

  // #cr_define_end
  return {
    AppManagementStoreClientImpl: AppManagementStoreClientImpl,
    AppManagementStoreClient: AppManagementStoreClient,
  };
});
