// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'app-management-app-item',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /** @type {App} */
    app: {
      type: Object,
    },
  },

  listeners: {
    'click': 'onClick_',
  },

  /**
   * @private
   */
  onClick_: function() {
    app_management.util.openAppDetailPage(this.app.id);
    chrome.metricsPrivate.recordEnumerationValue(
        AppManagementEntryPointsHistogramName,
        this.getAppManagementEntryPoint_(this.app.type),
        Object.keys(AppManagementEntryPoint).length);
  },

  /**
   * @param {App} app
   * @return {string}
   * @private
   */
  iconUrlFromId_: function(app) {
    return app_management.util.getAppIcon(app);
  },

  /**
   * @param {AppType} appType
   * @return {AppManagementEntryPoint}
   */
  getAppManagementEntryPoint_: function(appType) {
    switch (appType) {
      case AppType.kArc:
        return AppManagementEntryPoint.MainViewArc;
      case AppType.kExtension:
        return AppManagementEntryPoint.MainViewChromeApp;
      case AppType.kWeb:
        return AppManagementEntryPoint.MainViewWebApp;
      default:
        assertNotReached();
    }
  },
});
