// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'app-management-app',

  behaviors: [
    app_management.StoreClient,
  ],

  properties: {
    /** @private */
    searchTerm_: {
      type: String,
      observer: 'onSearchTermChanged_',
    },

    /**
     * @private {Page}
     */
    currentPage_: {
      type: Object,
    },
  },

  /**
   * @override
   */
  attached: function() {
    this.watch('searchTerm_', function(state) {
      return state.search.term;
    });
    this.watch('currentPage_', state => state.currentPage);
    this.updateFromStore();
  },

  /** @return {CrToolbarSearchFieldElement} */
  get searchField() {
    return /** @type {CrToolbarElement} */ (this.$$('cr-toolbar'))
        .getSearchField();
  },


  /** @private */
  onSearchTermChanged_: function() {
    this.searchField.setValue(this.searchTerm_ || '');
  },

  /**
   * @param {Event} e
   * @private
   */
  onSearchChanged_: function(e) {
    const searchTerm = /** @type {string} */ (e.detail);
    if (searchTerm != this.searchTerm_) {
      this.dispatch(app_management.actions.setSearchTerm(searchTerm));
    }
  },

  /**
   * @param {Page} currentPage
   * @param {String} searchTerm
   * @private
   */
  selectedRouteId_: function(currentPage, searchTerm) {
    if (searchTerm) {
      return 'search-view';
    }
    // This is to prevent console error caused by currentPage being undefined.
    if (currentPage) {
      switch (currentPage.pageType) {
        case (PageType.MAIN):
          return 'main-view';

        case (PageType.NOTIFICATIONS):
          return 'notifications-view';

        case (PageType.DETAIL):
          const state = this.getState();
          const selectedAppType =
              state.apps[assert(state.currentPage.selectedAppId)].type;
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

        default:
          assertNotReached();
      }
    }
  },
});
