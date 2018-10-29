// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history', function() {
  let lazyLoadPromise = null;
  function ensureLazyLoaded() {
    if (!lazyLoadPromise) {
      lazyLoadPromise = new Promise(function(resolve, reject) {
        Polymer.Base.importHref(
            'chrome://history/lazy_load.html', resolve, reject, true);
      });
    }
    return lazyLoadPromise;
  }

  return {
    ensureLazyLoaded: ensureLazyLoaded,
  };
});

Polymer({
  is: 'history-app',

  behaviors: [
    Polymer.IronScrollTargetBehavior,
  ],

  properties: {
    // The id of the currently selected page.
    selectedPage_: {
      type: String,
      observer: 'selectedPageChanged_',
    },

    /** @type {!QueryResult} */
    queryResult_: {
      type: Object,
      value: function() {
        return {
          info: null,
          results: null,
          sessionList: null,
        };
      }
    },

    isUserSignedIn_: {
      type: Boolean,
      // Updated on synced-device-manager attach by chrome.sending
      // 'otherDevicesInitialized'.
      value: loadTimeData.getBoolean('isUserSignedIn'),
    },

    toolbarShadow_: {
      type: Boolean,
      reflectToAttribute: true,
      notify: true,
    },

    showMenuPromo_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('showMenuPromo');
      },
    },

    /** @type {!QueryState} */
    queryState_: Object,

    // True if the window is narrow enough for the page to have a drawer.
    hasDrawer_: {
      type: Boolean,
      observer: 'hasDrawerChanged_',
    },

    // Used to display notices for profile sign-in status.
    showSidebarFooter: Boolean,
  },

  listeners: {
    'cr-toolbar-menu-promo-close': 'onCrToolbarMenuPromoClose_',
    'cr-toolbar-menu-promo-shown': 'onCrToolbarMenuPromoShown_',
    'cr-toolbar-menu-tap': 'onCrToolbarMenuTap_',
    'delete-selected': 'deleteSelected',
    'history-checkbox-select': 'checkboxSelected',
    'history-close-drawer': 'closeDrawer_',
    'history-view-changed': 'historyViewChanged_',
    'unselect-all': 'unselectAll',
  },

  /** @private {?function(!Event)} */
  boundOnCanExecute_: null,

  /** @private {?function(!Event)} */
  boundOnCommand_: null,

  /** @override */
  attached: function() {
    cr.ui.decorate('command', cr.ui.Command);
    this.boundOnCanExecute_ = this.onCanExecute_.bind(this);
    this.boundOnCommand_ = this.onCommand_.bind(this);

    document.addEventListener('canExecute', this.boundOnCanExecute_);
    document.addEventListener('command', this.boundOnCommand_);
  },

  /** @override */
  detached: function() {
    document.removeEventListener('canExecute', this.boundOnCanExecute_);
    document.removeEventListener('command', this.boundOnCommand_);
  },

  onFirstRender: function() {
    setTimeout(function() {
      chrome.send(
          'metricsHandler:recordTime',
          ['History.ResultsRenderedTime', window.performance.now()]);
    });

    // Focus the search field on load. Done here to ensure the history page
    // is rendered before we try to take focus.
    const searchField =
        /** @type {HistoryToolbarElement} */ (this.$.toolbar).searchField;
    if (!searchField.narrow) {
      searchField.getSearchInput().focus();
    }

    // Lazily load the remainder of the UI.
    md_history.ensureLazyLoaded().then(function() {
      window.requestIdleCallback(function() {
        document.fonts.load('bold 12px Roboto');
      });
    });
  },

  /** Overridden from IronScrollTargetBehavior */
  _scrollHandler: function() {
    if (this.scrollTarget)
      this.toolbarShadow_ = this.scrollTarget.scrollTop != 0;
  },

  /** @private */
  onCrToolbarMenuPromoClose_: function() {
    this.showMenuPromo_ = false;
  },

  /** @private */
  onCrToolbarMenuPromoShown_: function() {
    md_history.BrowserService.getInstance().menuPromoShown();
  },

  /** @private */
  onCrToolbarMenuTap_: function() {
    const drawer = /** @type {!CrDrawerElement} */ (this.$.drawer.get());
    drawer.toggle();
    this.showMenuPromo_ = false;
  },

  /**
   * Listens for history-item being selected or deselected (through checkbox)
   * and changes the view of the top toolbar.
   * @param {{detail: {countAddition: number}}} e
   */
  checkboxSelected: function(e) {
    const toolbar = /** @type {HistoryToolbarElement} */ (this.$.toolbar);
    toolbar.count = /** @type {HistoryListElement} */ (this.$.history)
                        .getSelectedItemCount();
  },

  selectOrUnselectAll: function() {
    const list = /** @type {HistoryListElement} */ (this.$.history);
    const toolbar = /** @type {HistoryToolbarElement} */ (this.$.toolbar);
    list.selectOrUnselectAll();
    toolbar.count = list.getSelectedItemCount();
  },

  /**
   * Listens for call to cancel selection and loops through all items to set
   * checkbox to be unselected.
   * @private
   */
  unselectAll: function() {
    const list = /** @type {HistoryListElement} */ (this.$.history);
    const toolbar = /** @type {HistoryToolbarElement} */ (this.$.toolbar);
    list.unselectAllItems();
    toolbar.count = 0;
  },

  deleteSelected: function() {
    this.$.history.deleteSelectedWithPrompt();
  },

  /**
   * @param {HistoryQuery} info An object containing information about the
   *    query.
   * @param {!Array<HistoryEntry>} results A list of results.
   */
  historyResult: function(info, results) {
    this.set('queryState_.querying', false);
    this.set('queryResult_.info', info);
    this.set('queryResult_.results', results);
    const list = /** @type {HistoryListElement} */ (this.$['history']);
    list.historyResult(info, results);
  },

  /**
   * Shows and focuses the search bar in the toolbar.
   */
  focusToolbarSearchField: function() {
    this.$.toolbar.showSearchField();
  },

  /**
   * @param {Event} e
   * @private
   */
  onCanExecute_: function(e) {
    e = /** @type {cr.ui.CanExecuteEvent} */ (e);
    switch (e.command.id) {
      case 'find-command':
      case 'slash-command':
        e.canExecute = !this.$.toolbar.searchField.isSearchFocused();
        break;
      case 'delete-command':
        e.canExecute = this.$.toolbar.count > 0;
        break;
      case 'select-all-command':
        e.canExecute = !this.$.toolbar.searchField.isSearchFocused() &&
            !this.syncedTabsSelected_(this.selectedPage_);
        break;
    }
  },

  /**
   * @param {Event} e
   * @private
   */
  onCommand_: function(e) {
    if (e.command.id == 'find-command' || e.command.id == 'slash-command')
      this.focusToolbarSearchField();
    else if (e.command.id == 'delete-command')
      this.deleteSelected();
    else if (e.command.id == 'select-all-command')
      this.selectOrUnselectAll();
  },

  /**
   * @param {!Array<!ForeignSession>} sessionList Array of objects describing
   *     the sessions from other devices.
   */
  setForeignSessions: function(sessionList) {
    this.set('queryResult_.sessionList', sessionList);
  },

  /**
   * Called when browsing data is cleared.
   */
  historyDeleted: function() {
    this.$.history.historyDeleted();
  },

  /**
   * Update sign in state of synced device manager after user logs in or out.
   * @param {boolean} isUserSignedIn
   */
  updateSignInState: function(isUserSignedIn) {
    this.isUserSignedIn_ = isUserSignedIn;
  },

  /**
   * @param {string} selectedPage
   * @return {boolean}
   * @private
   */
  syncedTabsSelected_: function(selectedPage) {
    return selectedPage == 'syncedTabs';
  },

  /**
   * @param {boolean} querying
   * @param {boolean} incremental
   * @param {string} searchTerm
   * @return {boolean} Whether a loading spinner should be shown (implies the
   *     backend is querying a new search term).
   * @private
   */
  shouldShowSpinner_: function(querying, incremental, searchTerm) {
    return querying && !incremental && searchTerm != '';
  },

  /** @private */
  selectedPageChanged_: function() {
    this.unselectAll();
    this.historyViewChanged_();
  },

  /** @private */
  historyViewChanged_: function() {
    // This allows the synced-device-manager to render so that it can be set as
    // the scroll target.
    requestAnimationFrame(() => {
      this._scrollHandler();
    });
    this.recordHistoryPageView_();
  },

  /** @private */
  hasDrawerChanged_: function() {
    const drawer =
        /** @type {?CrDrawerElement} */ (this.$.drawer.getIfExists());
    if (!this.hasDrawer_ && drawer && drawer.open)
      drawer.cancel();
  },

  /**
   * This computed binding is needed to make the iron-pages selector update when
   * the synced-device-manager is instantiated for the first time. Otherwise the
   * fallback selection will continue to be used after the corresponding item is
   * added as a child of iron-pages.
   * @param {string} selectedPage
   * @param {Array} items
   * @return {string}
   * @private
   */
  getSelectedPage_: function(selectedPage, items) {
    return selectedPage;
  },

  /** @private */
  closeDrawer_: function() {
    const drawer = this.$.drawer.get();
    if (drawer && drawer.open)
      drawer.close();
  },

  /** @private */
  recordHistoryPageView_: function() {
    let histogramValue = HistoryPageViewHistogram.END;
    switch (this.selectedPage_) {
      case 'syncedTabs':
        histogramValue = this.isUserSignedIn_ ?
            HistoryPageViewHistogram.SYNCED_TABS :
            HistoryPageViewHistogram.SIGNIN_PROMO;
        break;
      default:
        histogramValue = HistoryPageViewHistogram.HISTORY;
        break;
    }

    md_history.BrowserService.getInstance().recordHistogram(
        'History.HistoryPageView', histogramValue,
        HistoryPageViewHistogram.END);
  },
});
