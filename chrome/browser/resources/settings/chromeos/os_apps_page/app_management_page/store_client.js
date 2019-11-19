// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines StoreClient, a Polymer behavior to tie a front-end
 * element to back-end data from the store.
 */

cr.define('app_management', function() {
  /**
   * @polymerBehavior
   */
  const AppManagementStoreClientImpl = {
    /**
     * @param {string} localProperty
     * @param {function(!AppManagementPageState)} valueGetter
     */
    watch: function(localProperty, valueGetter) {
      this.watch_(localProperty, valueGetter);
    },

    /**
     * @return {AppManagementPageState}
     */
    getState: function() {
      return this.getStore().data;
    },

    /**
     * @return {cr.ui.Store<AppManagementPageState>}
     */
    getStore: function() {
      return app_management.Store.getInstance();
    },
  };

  /**
   * @polymerBehavior
   * @implements {cr.ui.StoreObserver}
   */
  const StoreClient = [cr.ui.StoreClient, AppManagementStoreClientImpl];

  return {
    AppManagementStoreClientImpl: AppManagementStoreClientImpl,
    StoreClient: StoreClient,
  };
});
