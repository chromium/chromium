// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-main-view',

  behaviors: [
    app_management.StoreClient,
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

  attached: function() {
    this.watch('apps_', state => state.apps);
    this.updateFromStore();
  },

  /**
   * @private
   * @param {Array<App>} appList
   * @return {boolean}
   */
  isAppListEmpty_: function(appList) {
    return appList.length === 0;
  },

  /**
   * @private
   * @param {AppMap} apps
   * @param {String} searchTerm
   * @return {Array<App>}
   */
  computeAppList_: function(apps, searchTerm) {
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
