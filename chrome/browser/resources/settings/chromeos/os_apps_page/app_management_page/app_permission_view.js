// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  // TODO(crbug.com/999016): change to app-management-app-detail-view.
  is: 'app-management-app-permission-view',

  behaviors: [
    app_management.StoreClient,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * @type {App}
     * @private
     */
    app_: {
      type: Object,
      observer: 'appChanged_',
    },

    /**
     * @type {string}
     * @private
     */
    selectedAppId_: {
      type: String,
      observer: 'selectedAppIdChanged_',
    },
  },

  attached: function() {
    this.watch('app_', state => app_management.util.getSelectedApp(state));
    this.watch('selectedAppId_', state => state.selectedAppId);
    this.updateFromStore();
  },

  detached: function() {
    this.dispatch(app_management.actions.updateSelectedAppId(null));
  },

  /**
   * Updates selected app ID based on the URL query params.
   *
   * settings.RouteObserverBehavior
   * @param {!settings.Route} currentRoute
   * @protected
   */
  currentRouteChanged: function(currentRoute) {
    if (currentRoute !== settings.routes.APP_MANAGEMENT_DETAIL) {
      return;
    }

    const appId = settings.getQueryParameters().get('id');
    const apps = this.getState().apps;

    if (!apps[appId] && Object.entries(apps).length !== 0) {
      // TODO(crbug.com/1010398): this call does not open the main page.
      app_management.util.openMainPage();
      return;
    }

    this.dispatch(app_management.actions.updateSelectedAppId(appId));
  },

  /**
   * @param {?App} app
   * @return {?string}
   * @private
   */
  getSelectedRouteId_: function(app) {
    if (!app) {
      return null;
    }

    const selectedAppType = app.type;
    switch (selectedAppType) {
      case (AppType.kWeb):
        return 'pwa-permission-view';
      case (AppType.kExtension):
        return 'chrome-app-permission-view';
      case (AppType.kArc):
        return 'arc-permission-view';
      default:
        assertNotReached();
    }
  },

  /**
   * @param {?App} app
   * @private
   */
  appChanged_: function(app) {
    if (app === null) {
      app_management.util.openMainPage();
    }
  },

  selectedAppIdChanged_: function(appId) {
    if (appId && this.app_) {
      app_management.util.recordAppManagementUserAction(
          this.app_.type, AppManagementUserAction.ViewOpened);
    }
  }
});
