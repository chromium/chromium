// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Module for functions which produce action objects. These are
 * listed in one place to document available actions and their parameters.
 */

cr.define('app_management.actions', function() {
  /**
   * @param {App} app
   */
  function addApp(app) {
    return {
      name: 'add-app',
      app: app,
    };
  }

  /**
   * @param {App} app
   */
  function changeApp(app) {
    return {
      name: 'change-app',
      app: app,
    };
  }

  /**
   * @param {string} id
   */
  function removeApp(id) {
    return {
      name: 'remove-app',
      id: id,
    };
  }

  /**
   * @param {?string} appId
   */
  function updateSelectedAppId(appId) {
    return {
      name: 'update-selected-app-id',
      value: appId,
    };
  }

  /**
   * @param {boolean} isSupported
   * @return {!cr.ui.Action}
   */
  function updateArcSupported(isSupported) {
    return {
      name: 'update-arc-supported',
      value: isSupported,
    };
  }


  return {
    addApp: addApp,
    changeApp: changeApp,
    removeApp: removeApp,
    updateArcSupported: updateArcSupported,
    updateSelectedAppId: updateSelectedAppId,
  };
});
