// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-list',

  behaviors: [I18nBehavior],

  properties: {
    // The search term for the current query. Set when the query returns.
    searchedTerm: {
      type: String,
      value: '',
    },

    resultLoadingDisabled_: {
      type: Boolean,
      value: false,
    },

    /**
     * Indexes into historyData_ of selected items.
     * @type {!Set<number>}
     */
    selectedItems: {
      type: Object,
      value: /** @return {!Set<string>} */ function() {
        return new Set();
      },
    },

    canDeleteHistory_: {
      type: Boolean,
      value: loadTimeData.getBoolean('allowDeletingHistory'),
    },

    // An array of history entries in reverse chronological order.
    historyData_: {
      type: Array,
      observer: 'onHistoryDataChanged_',
    },

    lastFocused_: Object,

    /** @private */
    listBlurred_: Boolean,

    lastSelectedIndex: Number,

    /** @type {!QueryState} */
    queryState: Object,

    /**
     * @private {?{
     *   index: number,
     *   item: !HistoryEntry,
     *   path: string,
     *   target: !HTMLElement
     * }}
     */
    actionMenuModel_: Object,
  },

  hostAttributes: {
    role: 'application',
  },

  listeners: {
    'history-checkbox-select': 'onItemSelected_',
    'open-menu': 'onOpenMenu_',
    'remove-bookmark-stars': 'onRemoveBookmarkStars_',
  },

  /** @override */
  attached: function() {
    // It is possible (eg, when middle clicking the reload button) for all other
    // resize events to fire before the list is attached and can be measured.
    // Adding another resize here ensures it will get sized correctly.
    /** @type {IronListElement} */ (this.$['infinite-list']).notifyResize();
    this.$['infinite-list'].scrollTarget = this;
    this.$['scroll-threshold'].scrollTarget = this;
    this.setAttribute('aria-roledescription', this.i18n('ariaRoleDescription'));
  },

  /////////////////////////////////////////////////////////////////////////////
  // Public methods:

  /**
   * @param {HistoryQuery} info An object containing information about the
   *    query.
   * @param {!Array<!HistoryEntry>} results A list of results.
   */
  historyResult: function(info, results) {
    this.initializeResults_(info, results);
    this.closeMenu_();

    if (info.term && !this.queryState.incremental) {
      Polymer.IronA11yAnnouncer.requestAvailability();
      this.fire('iron-announce', {
        text: history.HistoryItem.searchResultsTitle(results.length, info.term)
      });
    }

    this.addNewResults(results, this.queryState.incremental, info.finished);
  },

  /**
   * Adds the newly updated history results into historyData_. Adds new fields
   * for each result.
   * @param {!Array<!HistoryEntry>} historyResults The new history results.
   * @param {boolean} incremental Whether the result is from loading more
   * history, or a new search/list reload.
   * @param {boolean} finished True if there are no more results available and
   * result loading should be disabled.
   */
  addNewResults: function(historyResults, incremental, finished) {
    const results = historyResults.slice();
    /** @type {IronScrollThresholdElement} */ (this.$['scroll-threshold'])
        .clearTriggers();

    if (!incremental) {
      this.resultLoadingDisabled_ = false;
      if (this.historyData_) {
        this.splice('historyData_', 0, this.historyData_.length);
      }
      this.fire('unselect-all');
      this.scrollTop = 0;
    }

    if (this.historyData_) {
      // If we have previously received data, push the new items onto the
      // existing array.
      results.unshift('historyData_');
      this.push.apply(this, results);
    } else {
      // The first time we receive data, use set() to ensure the iron-list is
      // initialized correctly.
      this.set('historyData_', results);
    }

    this.resultLoadingDisabled_ = finished;
  },

  historyDeleted: function() {
    // Do not reload the list when there are items checked.
    if (this.getSelectedItemCount() > 0) {
      return;
    }

    // Reload the list with current search state.
    this.fire('query-history', false);
  },

  selectOrUnselectAll: function() {
    if (this.historyData_.length == this.getSelectedItemCount()) {
      this.unselectAllItems();
    } else {
      this.selectAllItems();
    }
  },

  /**
   * Select each item in |historyData|.
   */
  selectAllItems: function() {
    if (this.historyData_.length == this.getSelectedItemCount()) {
      return;
    }

    this.historyData_.forEach((item, index) => {
      this.changeSelection_(index, true);
    });
  },

  /**
   * Deselect each item in |selectedItems|.
   */
  unselectAllItems: function() {
    this.selectedItems.forEach((index) => {
      this.changeSelection_(index, false);
    });

    assert(this.selectedItems.size == 0);
  },

  /** @return {number} */
  getSelectedItemCount: function() {
    return this.selectedItems.size;
  },

  /**
   * Delete all the currently selected history items. Will prompt the user with
   * a dialog to confirm that the deletion should be performed.
   */
  deleteSelectedWithPrompt: function() {
    if (!this.canDeleteHistory_) {
      return;
    }

    const browserService = history.BrowserService.getInstance();
    browserService.recordAction('RemoveSelected');
    if (this.queryState.searchTerm != '') {
      browserService.recordAction('SearchResultRemove');
    }
    this.$.dialog.get().showModal();

    // TODO(dbeam): remove focus flicker caused by showModal() + focus().
    this.$$('.action-button').focus();
  },

  /////////////////////////////////////////////////////////////////////////////
  // Private methods:

  /**
   * Set the selection status for an item at a particular index.
   * @param {number} index
   * @param {boolean} selected
   * @private
   */
  changeSelection_: function(index, selected) {
    this.set(`historyData_.${index}.selected`, selected);
    if (selected) {
      this.selectedItems.add(index);
    } else {
      this.selectedItems.delete(index);
    }
  },

  /**
   * Performs a request to the backend to delete all selected items. If
   * successful, removes them from the view. Does not prompt the user before
   * deleting -- see deleteSelectedWithPrompt for a version of this method which
   * does prompt.
   * @private
   */
  deleteSelected_: function() {
    const toBeRemoved = Array.from(this.selectedItems.values())
                            .map((index) => this.get(`historyData_.${index}`));

    history.BrowserService.getInstance()
        .deleteItems(toBeRemoved)
        .then((items) => {
          this.removeItemsByIndex_(Array.from(this.selectedItems));
          this.fire('unselect-all');
          if (this.historyData_.length == 0) {
            // Try reloading if nothing is rendered.
            this.fire('query-history', false);
          }
        });
  },

  /**
   * Remove all |indices| from the history list. Uses notifySplices to send a
   * single large notification to Polymer, rather than many small notifications,
   * which greatly improves performance.
   * @param {!Array<number>} indices
   * @private
   */
  removeItemsByIndex_: function(indices) {
    const splices = [];
    indices.sort(function(a, b) {
      // Sort in reverse numerical order.
      return b - a;
    });
    indices.forEach((index) => {
      const item = this.historyData_.splice(index, 1);
      splices.push({
        index: index,
        removed: [item],
        addedCount: 0,
        object: this.historyData_,
        type: 'splice'
      });
    });
    this.notifySplices('historyData_', splices);
  },

  /**
   * Closes the overflow menu.
   * @private
   */
  closeMenu_: function() {
    const menu = this.$.sharedMenu.getIfExists();
    if (menu && menu.open) {
      this.actionMenuModel_ = null;
      menu.close();
    }
  },

  /////////////////////////////////////////////////////////////////////////////
  // Event listeners:

  /** @private */
  onDialogConfirmTap_: function() {
    history.BrowserService.getInstance().recordAction('ConfirmRemoveSelected');

    this.deleteSelected_();
    const dialog = assert(this.$.dialog.getIfExists());
    dialog.close();
  },

  /** @private */
  onDialogCancelTap_: function() {
    history.BrowserService.getInstance().recordAction('CancelRemoveSelected');

    const dialog = assert(this.$.dialog.getIfExists());
    dialog.close();
  },

  /**
   * Remove bookmark star for history items with matching URLs.
   * @param {!CustomEvent<string>} e
   * @private
   */
  onRemoveBookmarkStars_: function(e) {
    const url = e.detail;

    if (this.historyData_ === undefined) {
      return;
    }

    for (let i = 0; i < this.historyData_.length; i++) {
      if (this.historyData_[i].url == url) {
        this.set(`historyData_.${i}.starred`, false);
      }
    }
  },

  /**
   * Called when the page is scrolled to near the bottom of the list.
   * @private
   */
  onScrollToBottom_: function() {
    if (this.resultLoadingDisabled_ || this.queryState.querying) {
      return;
    }

    this.fire('query-history', true);
  },

  /**
   * Open the overflow menu and ensure that the item is visible in the scroll
   * pane when its menu is opened (it is possible to open off-screen items using
   * keyboard shortcuts).
   * @param {!CustomEvent<{
   *    index: number, item: !HistoryEntry,
   *    path: string, target: !HTMLElement
   * }>} e
   * @private
   */
  onOpenMenu_: function(e) {
    const index = e.detail.index;
    const list = /** @type {IronListElement} */ (this.$['infinite-list']);
    if (index < list.firstVisibleIndex || index > list.lastVisibleIndex) {
      list.scrollToIndex(index);
    }

    const target = e.detail.target;
    this.actionMenuModel_ = e.detail;
    const menu = /** @type {CrActionMenuElement} */ (this.$.sharedMenu.get());
    menu.showAt(target);
  },

  /** @private */
  onMoreFromSiteTap_: function() {
    history.BrowserService.getInstance().recordAction(
        'EntryMenuShowMoreFromSite');

    const menu = assert(this.$.sharedMenu.getIfExists());
    this.fire('change-query', {search: this.actionMenuModel_.item.domain});
    this.actionMenuModel_ = null;
    this.closeMenu_();
  },

  /** @private */
  onRemoveFromHistoryTap_: function() {
    const browserService = history.BrowserService.getInstance();
    browserService.recordAction('EntryMenuRemoveFromHistory');
    const menu = assert(this.$.sharedMenu.getIfExists());
    const itemData = this.actionMenuModel_;
    browserService.deleteItems([itemData.item]).then((items) => {
      // This unselect-all resets the toolbar when deleting a selected item
      // and clears selection state which can be invalid if items move
      // around during deletion.
      // TODO(tsergeant): Make this automatic based on observing list
      // modifications.
      this.fire('unselect-all');
      this.removeItemsByIndex_([itemData.index]);

      const index = itemData.index;
      if (index == undefined) {
        return;
      }
      setTimeout(() => {
        this.$['infinite-list'].focusItem(index);
        const item = getDeepActiveElement();
        if (item) {
          item.focusOnMenuButton();
        }
      });

      const browserService = history.BrowserService.getInstance();
      browserService.recordHistogram(
          'HistoryPage.RemoveEntryPosition',
          Math.min(index, UMA_MAX_BUCKET_VALUE), UMA_MAX_BUCKET_VALUE);
      if (index <= UMA_MAX_SUBSET_BUCKET_VALUE) {
        browserService.recordHistogram(
            'HistoryPage.RemoveEntryPositionSubset', index,
            UMA_MAX_SUBSET_BUCKET_VALUE);
      }
    });
    this.closeMenu_();
  },

  /**
   * @param {Event} e
   * @private
   */
  onItemSelected_: function(e) {
    const index = e.detail.index;
    const indices = [];

    // Handle shift selection. Change the selection state of all items between
    // |path| and |lastSelected| to the selection state of |item|.
    if (e.detail.shiftKey && this.lastSelectedIndex != undefined) {
      for (let i = Math.min(index, this.lastSelectedIndex);
           i <= Math.max(index, this.lastSelectedIndex); i++) {
        indices.push(i);
      }
    }

    if (indices.length == 0) {
      indices.push(index);
    }

    const selected = !this.selectedItems.has(index);

    indices.forEach((index) => {
      this.changeSelection_(index, selected);
    });

    this.lastSelectedIndex = index;
  },

  /////////////////////////////////////////////////////////////////////////////
  // Template helpers:

  /**
   * Check whether the time difference between the given history item and the
   * next one is large enough for a spacer to be required.
   * @param {HistoryEntry} item
   * @param {number} index The index of |item| in |historyData_|.
   * @return {boolean} Whether or not time gap separator is required.
   * @private
   */
  needsTimeGap_: function(item, index) {
    const length = this.historyData_.length;
    if (index === undefined || index >= length - 1 || length == 0) {
      return false;
    }

    const currentItem = this.historyData_[index];
    const nextItem = this.historyData_[index + 1];

    if (this.searchedTerm) {
      return currentItem.dateShort != nextItem.dateShort;
    }

    return currentItem.time - nextItem.time > BROWSING_GAP_TIME &&
        currentItem.dateRelativeDay == nextItem.dateRelativeDay;
  },

  /**
   * True if the given item is the beginning of a new card.
   * @param {HistoryEntry} item
   * @param {number} i Index of |item| within |historyData_|.
   * @return {boolean}
   * @private
   */
  isCardStart_: function(item, i) {
    const length = this.historyData_.length;
    if (i === undefined || length == 0 || i > length - 1) {
      return false;
    }
    return i == 0 ||
        this.historyData_[i].dateRelativeDay !=
        this.historyData_[i - 1].dateRelativeDay;
  },

  /**
   * True if the given item is the end of a card.
   * @param {HistoryEntry} item
   * @param {number} i Index of |item| within |historyData_|.
   * @return {boolean}
   * @private
   */
  isCardEnd_: function(item, i) {
    const length = this.historyData_.length;
    if (i === undefined || length == 0 || i > length - 1) {
      return false;
    }
    return i == length - 1 ||
        this.historyData_[i].dateRelativeDay !=
        this.historyData_[i + 1].dateRelativeDay;
  },

  /**
   * @return {boolean}
   * @private
   */
  hasResults_: function() {
    return this.historyData_.length > 0;
  },

  /**
   * @param {string} searchedTerm
   * @return {string}
   * @private
   */
  noResultsMessage_: function(searchedTerm) {
    const messageId = searchedTerm !== '' ? 'noSearchResults' : 'noResults';
    return loadTimeData.getString(messageId);
  },

  /**
   * @param {string} searchedTerm
   * @param {string} domain
   * @return {boolean}
   * @private
   */
  canSearchMoreFromSite_: function(searchedTerm, domain) {
    return searchedTerm === '' || searchedTerm !== domain;
  },

  /**
   * @param {HistoryQuery} info
   * @param {!Array<HistoryEntry>} results
   * @private
   */
  initializeResults_: function(info, results) {
    if (results.length == 0) {
      return;
    }

    let currentDate = results[0].dateRelativeDay;

    for (let i = 0; i < results.length; i++) {
      // Sets the default values for these fields to prevent undefined types.
      results[i].selected = false;
      results[i].readableTimestamp =
          info.term == '' ? results[i].dateTimeOfDay : results[i].dateShort;

      if (results[i].dateRelativeDay != currentDate) {
        currentDate = results[i].dateRelativeDay;
      }
    }
  },

  /**
   * Adding in order to address an issue with a flaky test. After the list is
   * updated, the test would not see the updated elements when using Polymer 2.
   * This has yet to be reproduced in manual testing.
   * @private
   */
  onHistoryDataChanged_: function() {
    this.$['infinite-list'].fire('iron-resize');
  },
});
