// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';
import './shared_style.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserService} from './browser_service.js';
import {BROWSING_GAP_TIME, UMA_MAX_BUCKET_VALUE, UMA_MAX_SUBSET_BUCKET_VALUE} from './constants.js';
import {HistoryEntry, HistoryQuery, QueryState} from './externs.js';
import {searchResultsTitle} from './history_item.js';

Polymer({
  is: 'history-list',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, WebUIListenerBehavior],

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

    pendingDelete: {
      notify: true,
      type: Boolean,
      value: false,
    },

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
  attached() {
    // It is possible (eg, when middle clicking the reload button) for all other
    // resize events to fire before the list is attached and can be measured.
    // Adding another resize here ensures it will get sized correctly.
    /** @type {IronListElement} */ (this.$['infinite-list']).notifyResize();
    this.$['infinite-list'].scrollTarget = this;
    this.$['scroll-threshold'].scrollTarget = this;
    this.setAttribute('aria-roledescription', this.i18n('ariaRoleDescription'));

    this.addWebUIListener('history-deleted', () => this.onHistoryDeleted_());
  },

  /////////////////////////////////////////////////////////////////////////////
  // Public methods:

  /**
   * @param {HistoryQuery} info An object containing information about the
   *    query.
   * @param {!Array<!HistoryEntry>} results A list of results.
   */
  historyResult(info, results) {
    this.initializeResults_(info, results);
    this.closeMenu_();

    if (info.term && !this.queryState.incremental) {
      IronA11yAnnouncer.requestAvailability();
      this.fire(
          'iron-announce',
          {text: searchResultsTitle(results.length, info.term)});
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
  addNewResults(historyResults, incremental, finished) {
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

  /** @private */
  onHistoryDeleted_() {
    // Do not reload the list when there are items checked.
    if (this.getSelectedItemCount() > 0) {
      return;
    }

    // Reload the list with current search state.
    this.fire('query-history', false);
  },

  selectOrUnselectAll() {
    if (this.historyData_.length === this.getSelectedItemCount()) {
      this.unselectAllItems();
    } else {
      this.selectAllItems();
    }
  },

  /**
   * Select each item in |historyData|.
   */
  selectAllItems() {
    if (this.historyData_.length === this.getSelectedItemCount()) {
      return;
    }

    this.historyData_.forEach((item, index) => {
      this.changeSelection_(index, true);
    });
  },

  /**
   * Deselect each item in |selectedItems|.
   */
  unselectAllItems() {
    this.selectedItems.forEach((index) => {
      this.changeSelection_(index, false);
    });

    assert(this.selectedItems.size === 0);

    IronA11yAnnouncer.requestAvailability();
    this.fire(
        'iron-announce', {text: loadTimeData.getString('itemsUnselected')});
  },

  /** @return {number} */
  getSelectedItemCount() {
    return this.selectedItems.size;
  },

  /**
   * Delete all the currently selected history items. Will prompt the user with
   * a dialog to confirm that the deletion should be performed.
   */
  deleteSelectedWithPrompt() {
    if (!this.canDeleteHistory_) {
      return;
    }

    const browserService = BrowserService.getInstance();
    browserService.recordAction('RemoveSelected');
    if (this.queryState.searchTerm !== '') {
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
  changeSelection_(index, selected) {
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
  deleteSelected_() {
    assert(!this.pendingDelete);

    const toBeRemoved = Array.from(this.selectedItems.values())
                            .map((index) => this.get(`historyData_.${index}`));

    this.deleteItems_(toBeRemoved).then(() => {
      this.pendingDelete = false;
      this.removeItemsByIndex_(Array.from(this.selectedItems));
      this.fire('unselect-all');
      if (this.historyData_.length === 0) {
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
  removeItemsByIndex_(indices) {
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
  closeMenu_() {
    const menu = this.$.sharedMenu.getIfExists();
    if (menu && menu.open) {
      this.actionMenuModel_ = null;
      menu.close();
    }
  },

  /////////////////////////////////////////////////////////////////////////////
  // Event listeners:

  /** @private */
  onDialogConfirmTap_() {
    BrowserService.getInstance().recordAction('ConfirmRemoveSelected');

    this.deleteSelected_();
    const dialog = assert(this.$.dialog.getIfExists());
    dialog.close();
  },

  /** @private */
  onDialogCancelTap_() {
    BrowserService.getInstance().recordAction('CancelRemoveSelected');

    const dialog = assert(this.$.dialog.getIfExists());
    dialog.close();
  },

  /**
   * Remove bookmark star for history items with matching URLs.
   * @param {!CustomEvent<string>} e
   * @private
   */
  onRemoveBookmarkStars_(e) {
    const url = e.detail;

    if (this.historyData_ === undefined) {
      return;
    }

    for (let i = 0; i < this.historyData_.length; i++) {
      if (this.historyData_[i].url === url) {
        this.set(`historyData_.${i}.starred`, false);
      }
    }
  },

  /**
   * Called when the page is scrolled to near the bottom of the list.
   * @private
   */
  onScrollToBottom_() {
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
  onOpenMenu_(e) {
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
  onMoreFromSiteTap_() {
    BrowserService.getInstance().recordAction('EntryMenuShowMoreFromSite');

    const menu = assert(this.$.sharedMenu.getIfExists());
    this.fire('change-query', {search: this.actionMenuModel_.item.domain});
    this.actionMenuModel_ = null;
    this.closeMenu_();
  },

  /**
   * @param {!Array<!HistoryEntry>} items
   * @return {!Promise}
   * @private
   */
  deleteItems_(items) {
    const removalList = items.map(item => ({
                                    url: item.url,
                                    timestamps: item.allTimestamps,
                                  }));

    this.pendingDelete = true;
    return BrowserService.getInstance().removeVisits(removalList);
  },

  /** @private */
  onRemoveFromHistoryTap_() {
    const browserService = BrowserService.getInstance();
    browserService.recordAction('EntryMenuRemoveFromHistory');

    assert(!this.pendingDelete);
    const menu = assert(this.$.sharedMenu.getIfExists());
    const itemData = this.actionMenuModel_;

    this.deleteItems_([itemData.item]).then(() => {
      // This unselect-all resets the toolbar when deleting a selected item
      // and clears selection state which can be invalid if items move
      // around during deletion.
      // TODO(tsergeant): Make this automatic based on observing list
      // modifications.
      this.pendingDelete = false;
      this.fire('unselect-all');
      this.removeItemsByIndex_([itemData.index]);

      const index = itemData.index;
      if (index === undefined) {
        return;
      }

      if (this.historyData_.length > 0) {
        setTimeout(() => {
          this.$['infinite-list'].focusItem(
              Math.min(this.historyData_.length - 1, index));
          const item = getDeepActiveElement();
          if (item && item.focusOnMenuButton) {
            item.focusOnMenuButton();
          }
        }, 1);
      }

      const browserService = BrowserService.getInstance();
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
  onItemSelected_(e) {
    const index = e.detail.index;
    const indices = [];

    // Handle shift selection. Change the selection state of all items between
    // |path| and |lastSelected| to the selection state of |item|.
    if (e.detail.shiftKey && this.lastSelectedIndex !== undefined) {
      for (let i = Math.min(index, this.lastSelectedIndex);
           i <= Math.max(index, this.lastSelectedIndex); i++) {
        indices.push(i);
      }
    }

    if (indices.length === 0) {
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
  needsTimeGap_(item, index) {
    const length = this.historyData_.length;
    if (index === undefined || index >= length - 1 || length === 0) {
      return false;
    }

    const currentItem = this.historyData_[index];
    const nextItem = this.historyData_[index + 1];

    if (this.searchedTerm) {
      return currentItem.dateShort !== nextItem.dateShort;
    }

    return currentItem.time - nextItem.time > BROWSING_GAP_TIME &&
        currentItem.dateRelativeDay === nextItem.dateRelativeDay;
  },

  /**
   * True if the given item is the beginning of a new card.
   * @param {HistoryEntry} item
   * @param {number} i Index of |item| within |historyData_|.
   * @return {boolean}
   * @private
   */
  isCardStart_(item, i) {
    const length = this.historyData_.length;
    if (i === undefined || length === 0 || i > length - 1) {
      return false;
    }
    return i === 0 ||
        this.historyData_[i].dateRelativeDay !==
        this.historyData_[i - 1].dateRelativeDay;
  },

  /**
   * True if the given item is the end of a card.
   * @param {HistoryEntry} item
   * @param {number} i Index of |item| within |historyData_|.
   * @return {boolean}
   * @private
   */
  isCardEnd_(item, i) {
    const length = this.historyData_.length;
    if (i === undefined || length === 0 || i > length - 1) {
      return false;
    }
    return i === length - 1 ||
        this.historyData_[i].dateRelativeDay !==
        this.historyData_[i + 1].dateRelativeDay;
  },

  /**
   * @return {boolean}
   * @private
   */
  hasResults_() {
    return this.historyData_.length > 0;
  },

  /**
   * @param {string} searchedTerm
   * @return {string}
   * @private
   */
  noResultsMessage_(searchedTerm) {
    const messageId = searchedTerm !== '' ? 'noSearchResults' : 'noResults';
    return loadTimeData.getString(messageId);
  },

  /**
   * @param {string} searchedTerm
   * @param {string} domain
   * @return {boolean}
   * @private
   */
  canSearchMoreFromSite_(searchedTerm, domain) {
    return searchedTerm === '' || searchedTerm !== domain;
  },

  /**
   * @param {HistoryQuery} info
   * @param {!Array<HistoryEntry>} results
   * @private
   */
  initializeResults_(info, results) {
    if (results.length === 0) {
      return;
    }

    let currentDate = results[0].dateRelativeDay;

    for (let i = 0; i < results.length; i++) {
      // Sets the default values for these fields to prevent undefined types.
      results[i].selected = false;
      results[i].readableTimestamp =
          info.term === '' ? results[i].dateTimeOfDay : results[i].dateShort;

      if (results[i].dateRelativeDay !== currentDate) {
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
  onHistoryDataChanged_() {
    this.$['infinite-list'].fire('iron-resize');
  },
});
