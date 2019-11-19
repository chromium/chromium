// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A singleton datastore for the App Management page. Page state
 * is publicly readable, but can only be modified by dispatching an Action to
 * the store.
 */

cr.define('app_management', function() {
  class Store extends cr.ui.Store {
    constructor() {
      super(
          app_management.util.createEmptyState(), app_management.reduceAction);
    }
  }

  cr.addSingletonGetter(Store);

  return {
    Store: Store,
  };
});
