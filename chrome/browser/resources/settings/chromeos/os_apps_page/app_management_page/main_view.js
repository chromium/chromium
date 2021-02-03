// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-main-view',

  behaviors: [
    app_management.AppManagementStoreClient,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * @type {string}
     */
    searchTerm: {
      type: String,
    },

    /**
     * @private {AppMap}
     */
    apps_: {
      type: Object,
    },

    /**
     * List of apps displayed.
     * @private {Array<App>}
     */
    appList_: {
      type: Array,
      value: () => [],
      computed: 'computeAppList_(apps_, searchTerm)'
    },
  },

  attached() {
    this.watch('apps_', state => state.apps);
    this.updateFromStore();
  },

  /**
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    if (route === settings.routes.APP_MANAGEMENT) {
      const appId =
          app_management.AppManagementStore.getInstance().data.selectedAppId;

      // Expect this to be false the first time the "Manage your apps" page
      // is requested as no app has been selected yet.
      if (appId) {
        const button = this.$$(`#app-subpage-button-${appId}`);
        if (button) {
          cr.ui.focusWithoutInk(button);
        }
      }
    }
  },

  /**
   * @private
   * @param {Array<App>} appList
   * @return {boolean}
   */
  isAppListEmpty_(appList) {
    return appList.length === 0;
  },

  /**
   * @private
   * @param {AppMap} apps
   * @param {String} searchTerm
   * @return {Array<App>}
   */
  computeAppList_(apps, searchTerm) {
    if (!apps) {
      return [];
    }

    // This is calculated locally as once the user leaves this page the state
    // should reset.
    const appArray = Object.values(apps);

    let filteredApps;
    if (searchTerm) {
      const lowerCaseSearchTerm = searchTerm.toLowerCase();
      filteredApps = appArray.filter(app => {
        assert(app.title);
        return app.title.toLowerCase().includes(lowerCaseSearchTerm);
      });
    } else {
      filteredApps = appArray;
    }

    filteredApps.sort(
        (a, b) => app_management.util.alphabeticalSort(
            /** @type {string} */ (a.title), /** @type {string} */ (b.title)));

    return filteredApps;
  },
});
