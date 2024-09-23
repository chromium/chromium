// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';
import './shared_style.css.js';
import './history_item.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import type {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import type {IronScrollThresholdElement} from 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserServiceImpl} from './browser_service.js';
import {BROWSING_GAP_TIME} from './constants.js';
import type {HistoryEntry, HistoryQuery, QueryState} from './externs.js';
import type {HistoryItemElement} from './history_item.js';
import {searchResultsTitle} from './history_item.js';
import {getTemplate} from './history_list.html.js';

export interface ActionMenuModel {
  index: number;
  item: HistoryEntry;
  target: HTMLElement;
}

type OpenMenuEvent = CustomEvent<ActionMenuModel>;

type HistoryCheckboxSelectEvent = CustomEvent<{
  index: number,
  shiftKey: boolean,
}>;

export interface HistoryListElement {
  $: {
    'infinite-list': IronListElement,
    'scroll-threshold': IronScrollThresholdElement,
    'dialog': CrLazyRenderElement<CrDialogElement>,
    'no-results': HTMLElement,
    'sharedMenu': CrLazyRenderElement<CrActionMenuElement>,
  };
}

declare global {
  interface HTMLElementEventMap {
    'history-checkbox-select': HistoryCheckboxSelectEvent;
    'open-menu': OpenMenuEvent;
    'remove-bookmark-stars': CustomEvent<string>;
  }
}

const HistoryListElementBase = WebUiListenerMixin(I18nMixin(PolymerElement));

export class HistoryListElement extends HistoryListElementBase {
  static get is() {
    return 'history-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // The search term for the current query. Set when the query returns.
      searchedTerm: String,

      resultLoadingDisabled_: Boolean,

      /**
       * Indexes into historyData_ of selected items.
       */
      selectedItems: Object,

      canDeleteHistory_: Boolean,

      // An array of history entries in reverse chronological order.
      historyData_: {
        type: Array,
        observer: 'onHistoryDataChanged_',
      },

      lastFocused_: Object,

      listBlurred_: Boolean,

      lastSelectedIndex: Number,

      pendingDelete: {
        notify: true,
        type: Boolean,
      },

      queryState: Object,

      actionMenuModel_: Object,

      scrollTarget: {
        type: Object,
        observer: 'onScrollTargetChanged_',
      },
      scrollOffset: Number,

      isEmpty: {
        type: Boolean,
        reflectToAttribute: true,
        computed: 'computeIsEmpty_(historyData_.length)',
      },
    };
  }

  private historyData_: HistoryEntry[];
  private canDeleteHistory_: boolean =
      loadTimeData.getBoolean('allowDeletingHistory');
  private actionMenuModel_: ActionMenuModel|null = null;
  private resultLoadingDisabled_: boolean = false;
  isEmpty: boolean;
  searchedTerm: string = '';
  selectedItems: Set<number> = new Set();
  pendingDelete: boolean = false;
  lastSelectedIndex: number;
  queryState: QueryState;
  scrollTarget: HTMLElement = document.documentElement;
  scrollOffset: number = 0;

  override connectedCallback() {
    super.connectedCallback();
    this.setAttribute('aria-roledescription', this.i18n('ariaRoleDescription'));
    this.addWebUiListener('history-deleted', () => this.onHistoryDeleted_());
  }

  override ready() {
    super.ready();

    this.setAttribute('role', 'application');

    this.addEventListener('history-checkbox-select', this.onItemSelected_);
    this.addEventListener('open-menu', this.onOpenMenu_);
    this.addEventListener(
        'remove-bookmark-stars', e => this.onRemoveBookmarkStars_(e));
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  /////////////////////////////////////////////////////////////////////////////
  // Public methods:

  /**
   * @param info An object containing information about the query.
   * @param results A list of results.
   */
  historyResult(info: HistoryQuery, results: HistoryEntry[]) {
    this.initializeResults_(info, results);
    this.closeMenu_();

    if (info.term && !this.queryState.incremental) {
      getAnnouncerInstance().announce(
          searchResultsTitle(results.length, info.term));
    }

    this.addNewResults(results, this.queryState.incremental, info.finished);
  }

  /**
   * Adds the newly updated history results into historyData_. Adds new fields
   * for each result.
   * @param historyResults The new history results.
   * @param incremental Whether the result is from loading more history, or a
   *     new search/list reload.
   * @param finished True if there are no more results available and result
   *     loading should be disabled.
   */
  addNewResults(
      historyResults: HistoryEntry[], incremental: boolean, finished: boolean) {
    const results = historyResults.slice();
    this.$['scroll-threshold'].clearTriggers();

    if (!incremental) {
      this.resultLoadingDisabled_ = false;
      if (this.historyData_) {
        this.splice('historyData_', 0, this.historyData_.length);
      }
      this.fire_('unselect-all');
      this.scrollTop = 0;
    }

    if (this.historyData_) {
      // If we have previously received data, push the new items onto the
      // existing array.
      this.push('historyData_', ...results);
    } else {
      // The first time we receive data, use set() to ensure the iron-list is
      // initialized correctly.
      this.set('historyData_', results);
    }

    this.resultLoadingDisabled_ = finished;
  }

  private onHistoryDeleted_() {
    // Do not reload the list when there are items checked.
    if (this.getSelectedItemCount() > 0) {
      return;
    }

    // Reload the list with current search state.
    this.fire_('query-history', false);
  }

  selectOrUnselectAll() {
    if (this.historyData_.length === this.getSelectedItemCount()) {
      this.unselectAllItems();
    } else {
      this.selectAllItems();
    }
  }

  /**
   * Select each item in |historyData|.
   */
  selectAllItems() {
    if (this.historyData_.length === this.getSelectedItemCount()) {
      return;
    }

    this.historyData_.forEach((_item, index) => {
      this.changeSelection_(index, true);
    });
  }

  /**
   * Deselect each item in |selectedItems|.
   */
  unselectAllItems() {
    this.selectedItems.forEach((index) => {
      this.changeSelection_(index, false);
    });

    assert(this.selectedItems.size === 0);
  }

  /** @return {number} */
  getSelectedItemCount() {
    return this.selectedItems.size;
  }

  /**
   * Delete all the currently selected history items. Will prompt the user with
   * a dialog to confirm that the deletion should be performed.
   */
  deleteSelectedWithPrompt() {
    if (!this.canDeleteHistory_) {
      return;
    }

    const browserService = BrowserServiceImpl.getInstance();
    browserService.recordAction('RemoveSelected');
    if (this.queryState.searchTerm !== '') {
      browserService.recordAction('SearchResultRemove');
    }
    this.$.dialog.get().showModal();

    // TODO(dbeam): remove focus flicker caused by showModal() + focus().
    const button =
        this.shadowRoot!.querySelector<HTMLElement>('.action-button');
    assert(button);
    button.focus();
  }

  // Notifies the iron-list of this element being potentially resized.
  notifyResize() {
    this.$['infinite-list'].notifyResize();
  }

  /////////////////////////////////////////////////////////////////////////////
  // Private methods:

  /**
   * Set the selection status for an item at a particular index.
   */
  private changeSelection_(index: number, selected: boolean) {
    this.set(`historyData_.${index}.selected`, selected);
    if (selected) {
      this.selectedItems.add(index);
    } else {
      this.selectedItems.delete(index);
    }
  }

  /**
   * Performs a request to the backend to delete all selected items. If
   * successful, removes them from the view. Does not prompt the user before
   * deleting -- see deleteSelectedWithPrompt for a version of this method which
   * does prompt.
   */
  private deleteSelected_() {
    assert(!this.pendingDelete);

    const toBeRemoved = Array.from(this.selectedItems.values())
                            .map((index) => this.get(`historyData_.${index}`));

    this.deleteItems_(toBeRemoved).then(() => {
      this.pendingDelete = false;
      this.removeItemsByIndex_(Array.from(this.selectedItems));
      this.fire_('unselect-all');
      if (this.historyData_.length === 0) {
        // Try reloading if nothing is rendered.
        this.fire_('query-history', false);
      }
    });
  }

  removeItemsForTest(indices: number[]) {
    this.removeItemsByIndex_(indices);
  }

  /**
   * Remove all |indices| from the history list. Uses notifySplices to send a
   * single large notification to Polymer, rather than many small notifications,
   * which greatly improves performance.
   */
  private removeItemsByIndex_(indices: number[]) {
    const splices: any[] = [];
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
        type: 'splice',
      });
    });
    this.notifySplices('historyData_', splices);
  }

  removeItemsByIndexForTesting(indices: number[]) {
    this.removeItemsByIndex_(indices);
  }

  /**
   * Closes the overflow menu.
   */
  private closeMenu_() {
    const menu = this.$.sharedMenu.getIfExists();
    if (menu && menu.open) {
      this.actionMenuModel_ = null;
      menu.close();
    }
  }

  /////////////////////////////////////////////////////////////////////////////
  // Event listeners:

  private onDialogConfirmClick_() {
    BrowserServiceImpl.getInstance().recordAction('ConfirmRemoveSelected');

    this.deleteSelected_();
    const dialog = this.$.dialog.getIfExists();
    assert(dialog);
    dialog.close();
  }

  private onDialogCancelClick_() {
    BrowserServiceImpl.getInstance().recordAction('CancelRemoveSelected');

    const dialog = this.$.dialog.getIfExists();
    assert(dialog);
    dialog.close();
  }

  /**
   * Remove bookmark star for history items with matching URLs.
   */
  private onRemoveBookmarkStars_(e: CustomEvent<string>) {
    const url = e.detail;

    if (this.historyData_ === undefined) {
      return;
    }

    for (let i = 0; i < this.historyData_.length; i++) {
      if (this.historyData_[i].url === url) {
        this.set(`historyData_.${i}.starred`, false);
      }
    }
  }

  /**
   * Called when the page is scrolled to near the bottom of the list.
   */
  private onScrollToBottom_() {
    if (this.resultLoadingDisabled_ || this.queryState.querying) {
      return;
    }

    this.fire_('query-history', true);
  }

  /**
   * Open the overflow menu and ensure that the item is visible in the scroll
   * pane when its menu is opened (it is possible to open off-screen items using
   * keyboard shortcuts).
   */
  private onOpenMenu_(e: OpenMenuEvent) {
    const index = e.detail.index;
    const list = this.$['infinite-list'];
    if (index < list.firstVisibleIndex || index > list.lastVisibleIndex) {
      list.scrollToIndex(index);
    }

    const target = e.detail.target;
    this.actionMenuModel_ = e.detail;
    this.$.sharedMenu.get().showAt(target);
  }

  private onMoreFromSiteClick_() {
    BrowserServiceImpl.getInstance().recordAction('EntryMenuShowMoreFromSite');

    assert(this.$.sharedMenu.getIfExists());
    this.fire_(
        'change-query', {search: 'host:' + this.actionMenuModel_!.item.domain});
    this.actionMenuModel_ = null;
    this.closeMenu_();
  }

  private deleteItems_(items: HistoryEntry[]): Promise<void> {
    const removalList = items.map(item => ({
                                    url: item.url,
                                    timestamps: item.allTimestamps,
                                  }));

    this.pendingDelete = true;
    return BrowserServiceImpl.getInstance().removeVisits(removalList);
  }

  private onRemoveBookmarkClick_() {
    const browserService = BrowserServiceImpl.getInstance();
    browserService.removeBookmark(this.actionMenuModel_!.item.url);
    this.fire_('remove-bookmark-stars', this.actionMenuModel_!.item.url);
    this.closeMenu_();
  }

  private onRemoveFromHistoryClick_() {
    const browserService = BrowserServiceImpl.getInstance();
    browserService.recordAction('EntryMenuRemoveFromHistory');

    assert(!this.pendingDelete);
    assert(this.$.sharedMenu.getIfExists());
    const itemData = this.actionMenuModel_!;

    this.deleteItems_([itemData.item]).then(() => {
      getAnnouncerInstance().announce(
          this.i18n('deleteSuccess', itemData.item.title));

      // This unselect-all resets the toolbar when deleting a selected item
      // and clears selection state which can be invalid if items move
      // around during deletion.
      // TODO(tsergeant): Make this automatic based on observing list
      // modifications.
      this.pendingDelete = false;
      this.fire_('unselect-all');
      this.removeItemsByIndex_([itemData.index]);

      const index = itemData.index;
      if (index === undefined) {
        return;
      }

      if (this.historyData_.length > 0) {
        setTimeout(() => {
          this.$['infinite-list'].focusItem(
              Math.min(this.historyData_.length - 1, index));
          const item = getDeepActiveElement() as HistoryItemElement;
          if (item && item.focusOnMenuButton) {
            item.focusOnMenuButton();
          }
        }, 1);
      }
    });
    this.closeMenu_();
  }

  private onItemSelected_(e: HistoryCheckboxSelectEvent) {
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
  }

  /////////////////////////////////////////////////////////////////////////////
  // Template helpers:

  /**
   * Check whether the time difference between the given history item and the
   * next one is large enough for a spacer to be required.
   */
  private needsTimeGap_(_item: HistoryEntry, index: number): boolean {
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
  }

  /**
   * True if the given item is the beginning of a new card.
   * @param i Index of |item| within |historyData_|.
   */
  private isCardStart_(_item: HistoryEntry, i: number): boolean {
    const length = this.historyData_.length;
    if (i === undefined || length === 0 || i > length - 1) {
      return false;
    }
    return i === 0 ||
        this.historyData_[i].dateRelativeDay !==
        this.historyData_[i - 1].dateRelativeDay;
  }

  /**
   * True if the given item is the end of a card.
   * @param i Index of |item| within |historyData_|.
   */
  private isCardEnd_(_item: HistoryEntry, i: number): boolean {
    const length = this.historyData_.length;
    if (i === undefined || length === 0 || i > length - 1) {
      return false;
    }
    return i === length - 1 ||
        this.historyData_[i].dateRelativeDay !==
        this.historyData_[i + 1].dateRelativeDay;
  }

  private hasResults_(): boolean {
    return this.historyData_.length > 0;
  }

  private noResultsMessage_(searchedTerm: string): string {
    const messageId = searchedTerm !== '' ? 'noSearchResults' : 'noResults';
    return loadTimeData.getString(messageId);
  }

  private canSearchMoreFromSite_(searchedTerm: string, domain: string):
      boolean {
    return searchedTerm === '' || searchedTerm !== domain;
  }

  private initializeResults_(info: HistoryQuery, results: HistoryEntry[]) {
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
  }

  /**
   * Adding in order to address an issue with a flaky test. After the list is
   * updated, the test would not see the updated elements when using Polymer 2.
   * This has yet to be reproduced in manual testing.
   */
  private onHistoryDataChanged_() {
    this.$['infinite-list'].fire('iron-resize');
  }

  private getHistoryEmbeddingsMatches_(): HistoryEntry[] {
    return this.historyData_.slice(0, 3);
  }

  private showHistoryEmbeddings_(): boolean {
    return loadTimeData.getBoolean('enableHistoryEmbeddings') &&
        !!this.searchedTerm && this.historyData_?.length > 0;
  }

  private onScrollTargetChanged_() {
    // It is possible (eg, when middle clicking the reload button) for all other
    // resize events to fire before the list is attached and can be measured.
    // Adding another resize here ensures it will get sized correctly.
    this.$['infinite-list'].notifyResize();
  }

  private computeIsEmpty_() {
    return this.historyData_.length === 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-list': HistoryListElement;
  }
}

customElements.define(HistoryListElement.is, HistoryListElement);
