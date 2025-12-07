// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_infinite_list/cr_infinite_list.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import './history_item.js';

import type {HistoryEntry, HistoryQuery, PageCallbackRouter, PageHandlerRemote, QueryState} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInfiniteListElement} from 'chrome://resources/cr_elements/cr_infinite_list/cr_infinite_list.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {BrowserService} from './browser_service.js';
import {BrowserServiceImpl} from './browser_service.js';
import {BROWSING_GAP_TIME, VisitContextMenuAction} from './constants.js';
import type {HistoryItemElement} from './history_item.js';
import {getCss} from './history_list.css.js';
import {getHtml} from './history_list.html.js';

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
    infiniteList: CrInfiniteListElement,
    dialog: CrLazyRenderLitElement<CrDialogElement>,
    noResults: HTMLElement,
    sharedMenu: CrLazyRenderLitElement<CrActionMenuElement>,
  };
}

declare global {
  interface HTMLElementEventMap {
    'history-checkbox-select': HistoryCheckboxSelectEvent;
    'open-menu': OpenMenuEvent;
    'remove-bookmark-stars': CustomEvent<string>;
  }
}

const HistoryListElementBase = I18nMixinLit(CrLitElement);

export class HistoryListElement extends HistoryListElementBase {
  static get is() {
    return 'history-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // The search term for the current query. Set when the query returns.
      searchedTerm: {type: String},

      resultLoadingDisabled_: {type: Boolean},

      /**
       * Indexes into historyData_ of selected items.
       */
      selectedItems: {type: Object},

      canDeleteHistory_: {type: Boolean},

      // An array of history entries in reverse chronological order.
      historyData_: {type: Array},

      lastFocused_: {type: Object},

      listBlurred_: {type: Boolean},

      lastSelectedIndex: {type: Number},

      pendingDelete: {
        notify: true,
        type: Boolean,
      },

      queryState: {type: Object},

      actionMenuModel_: {type: Object},

      scrollTarget: {type: Object},
      scrollOffset: {type: Number},

      // Whether this element is active, i.e. visible to the user.
      isActive: {type: Boolean},

      isEmpty: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  protected accessor historyData_: HistoryEntry[] = [];
  private browserService_: BrowserService = BrowserServiceImpl.getInstance();
  private callbackRouter_: PageCallbackRouter =
      BrowserServiceImpl.getInstance().callbackRouter;
  protected accessor canDeleteHistory_: boolean =
      loadTimeData.getBoolean('allowDeletingHistory');
  protected accessor actionMenuModel_: ActionMenuModel|null = null;
  private lastOffsetHeight_: number = 0;
  private pageHandler_: PageHandlerRemote =
      BrowserServiceImpl.getInstance().handler;
  private resizeObserver_: ResizeObserver = new ResizeObserver(() => {
    if (this.lastOffsetHeight_ === 0) {
      this.lastOffsetHeight_ = this.scrollTarget.offsetHeight;
      return;
    }
    if (this.scrollTarget.offsetHeight > this.lastOffsetHeight_) {
      this.lastOffsetHeight_ = this.scrollTarget.offsetHeight;
      this.onScrollOrResize_();
    }
  });
  private accessor resultLoadingDisabled_: boolean = false;
  private scrollDebounce_: number = 200;
  private scrollListener_: EventListener = () => this.onScrollOrResize_();
  private scrollTimeout_: number|null = null;
  accessor isActive: boolean = true;
  accessor isEmpty: boolean = false;
  accessor searchedTerm: string = '';
  accessor selectedItems: Set<number> = new Set();
  accessor pendingDelete: boolean = false;
  protected accessor lastFocused_: HTMLElement|null;
  protected accessor listBlurred_: boolean;
  accessor lastSelectedIndex: number = -1;
  accessor queryState: QueryState;
  accessor scrollTarget: HTMLElement = document.documentElement;
  accessor scrollOffset: number = 0;
  private onHistoryDeletedListenerId_: number|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.onHistoryDeletedListenerId_ =
        this.callbackRouter_.onHistoryDeleted.addListener(
            this.onHistoryDeleted_.bind(this));
  }

  override firstUpdated() {
    this.setAttribute('role', 'application');
    this.setAttribute('aria-roledescription', this.i18n('ariaRoleDescription'));

    this.addEventListener('history-checkbox-select', this.onItemSelected_);
    this.addEventListener('open-menu', this.onOpenMenu_);
    this.addEventListener(
        'remove-bookmark-stars', e => this.onRemoveBookmarkStars_(e));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('historyData_')) {
      this.isEmpty = this.historyData_.length === 0;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('isActive')) {
      this.onIsActiveChanged_();
    }

    if (changedProperties.has('scrollTarget')) {
      this.onScrollTargetChanged_(changedProperties.get('scrollTarget'));
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.onHistoryDeletedListenerId_);
    this.callbackRouter_.removeListener(this.onHistoryDeletedListenerId_);
    this.onHistoryDeletedListenerId_ = null;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Public methods:

  /**
   * @param info An object containing information about the query.
   * @param results A list of results.
   */
  historyResult(info: HistoryQuery, results: HistoryEntry[]) {
    if (!info) {
      // Canceled results for an outdated query has null query info.
      return;
    }

    this.initializeResults_(info, results);
    this.closeMenu_();

    if (info.term && !this.queryState.incremental) {
      let resultsLabelId;
      if (loadTimeData.getBoolean('enableHistoryEmbeddings')) {
        // Differentiate screen reader messages if embeddings are enabled so
        // that the messages specify these are results for exact matches not
        // embeddings results.
        resultsLabelId = results.length === 1 ? 'searchResultExactMatch' :
                                                'searchResultExactMatches';
      } else {
        resultsLabelId =
            results.length === 1 ? 'searchResult' : 'searchResults';
      }
      const message = loadTimeData.getStringF(
          'foundSearchResults', results.length,
          loadTimeData.getString(resultsLabelId), info.term);
      getAnnouncerInstance().announce(message);
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
    if (this.scrollTimeout_) {
      clearTimeout(this.scrollTimeout_);
    }

    if (!incremental) {
      this.resultLoadingDisabled_ = false;
      this.historyData_ = [];
      this.fire('unselect-all');
      this.scrollTarget.scrollTop = 0;
    }

    this.historyData_ = [...this.historyData_, ...results];
    this.resultLoadingDisabled_ = finished;

    if (loadTimeData.getBoolean('enableBrowsingHistoryActorIntegrationM1')) {
      this.recordActorVisitShown_(results);
    }
  }

  private recordActorVisitShown_(historyResults: HistoryEntry[]) {
    const historyResultsContainActorVisit =
        historyResults.some((result) => result.isActorVisit);

    this.browserService_.recordBooleanHistogram(
        'HistoryPage.ActorItemsShown', historyResultsContainActorVisit);
  }

  private onHistoryDeleted_() {
    // Do not reload the list when there are items checked.
    if (this.getSelectedItemCount() > 0) {
      return;
    }

    // Reload the list with current search state.
    this.fire('query-history', false);
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

    const indices = this.historyData_.map((_, index) => index);
    this.changeSelections_(indices, true);
  }

  /**
   * Deselect each item in |selectedItems|.
   */
  unselectAllItems() {
    this.changeSelections_(Array.from(this.selectedItems), false);
    assert(this.selectedItems.size === 0);
  }

  getSelectedItemCount(): number {
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

    this.browserService_.recordAction('RemoveSelected');
    if (this.queryState.searchTerm !== '') {
      this.browserService_.recordAction('SearchResultRemove');
    }
    this.$.dialog.get().showModal();

    // TODO(dbeam): remove focus flicker caused by showModal() + focus().
    const button = this.shadowRoot.querySelector<HTMLElement>('.action-button');
    assert(button);
    button.focus();
  }

  fillCurrentViewport() {
    this.$.infiniteList.fillCurrentViewport();
  }

  openSelected() {
    const selected = this.getSelectedEntries_();

    for (const entry of selected) {
      window.open(entry.url, '_blank');
    }
  }

  /////////////////////////////////////////////////////////////////////////////
  // Private methods:

  /**
   * Set the selection status for a set of items by their indices.
   */
  private changeSelections_(indices: number[], selected: boolean) {
    indices.forEach(index => {
      if (this.historyData_[index]) {
        this.historyData_[index].selected = selected;
      }

      if (selected) {
        this.selectedItems.add(index);
      } else {
        this.selectedItems.delete(index);
      }
    });
    this.requestUpdate();
  }

  /**
   * Performs a request to the backend to delete all selected items. If
   * successful, removes them from the view. Does not prompt the user before
   * deleting -- see deleteSelectedWithPrompt for a version of this method which
   * does prompt.
   */
  private deleteSelected_() {
    assert(!this.pendingDelete);

    const toBeRemoved = this.getSelectedEntries_();

    this.deleteItems_(toBeRemoved).then(() => {
      this.pendingDelete = false;
      this.removeItemsByIndex_(Array.from(this.selectedItems));
      this.fire('unselect-all');
      if (this.historyData_.length === 0) {
        // Try reloading if nothing is rendered.
        this.fire('query-history', false);
      }
    });
  }

  /**
   * Remove all |indices| from the history list.
   */
  private removeItemsByIndex_(indices: number[]) {
    const indicesSet = new Set(indices);
    this.historyData_ =
        this.historyData_.filter((_, index) => !indicesSet.has(index));
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

  protected onDialogConfirmClick_() {
    this.browserService_.recordAction('ConfirmRemoveSelected');

    this.deleteSelected_();
    const dialog = this.$.dialog.getIfExists();
    assert(dialog);
    dialog.close();
  }

  protected onDialogCancelClick_() {
    this.browserService_.recordAction('CancelRemoveSelected');

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

    this.historyData_.forEach(data => {
      if (data.url === url) {
        data.starred = false;
      }
    });
    this.requestUpdate();
  }

  /**
   * Called when the page is scrolled to near the bottom of the list.
   */
  private onScrollToBottom_() {
    if (this.resultLoadingDisabled_ || this.queryState.querying) {
      return;
    }

    this.fire('query-history', true);
  }

  private onOpenMenu_(e: OpenMenuEvent) {
    const target = e.detail.target;
    this.actionMenuModel_ = e.detail;
    this.$.sharedMenu.get().showAt(target);
  }

  private deleteItems_(items: HistoryEntry[]): Promise<void> {
    const removalList = items.map(item => ({
                                    url: item.url,
                                    timestamps: item.allTimestamps,
                                  }));

    this.pendingDelete = true;
    return this.pageHandler_.removeVisits(removalList);
  }

  private recordContextMenuActionsHistogram_(action: VisitContextMenuAction) {
    if (!loadTimeData.getBoolean('enableBrowsingHistoryActorIntegrationM1')) {
      return;
    }

    this.browserService_.recordHistogram(
        this.actionMenuModel_!.item.isActorVisit ?
            'HistoryPage.ActorContextMenuActions' :
            'HistoryPage.NonActorContextMenuActions',
        action, VisitContextMenuAction.MAX_VALUE);
  }

  protected onMoreFromSiteClick_() {
    this.browserService_.recordAction('EntryMenuShowMoreFromSite');
    this.recordContextMenuActionsHistogram_(
        VisitContextMenuAction.MORE_FROM_THIS_SITE_CLICKED);


    assert(this.$.sharedMenu.getIfExists());
    this.fire(
        'change-query', {search: 'host:' + this.actionMenuModel_!.item.domain});
    this.actionMenuModel_ = null;
    this.closeMenu_();
  }

  protected onRemoveBookmarkClick_() {
    this.recordContextMenuActionsHistogram_(
        VisitContextMenuAction.REMOVE_BOOKMARK_CLICKED);

    this.pageHandler_.removeBookmark(this.actionMenuModel_!.item.url);
    this.fire('remove-bookmark-stars', this.actionMenuModel_!.item.url);
    this.closeMenu_();
  }

  protected onRemoveFromHistoryClick_() {
    this.browserService_.recordAction('EntryMenuRemoveFromHistory');
    this.recordContextMenuActionsHistogram_(
        VisitContextMenuAction.REMOVE_FROM_HISTORY_CLICKED);

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
      this.fire('unselect-all');
      this.removeItemsByIndex_([itemData.index]);

      const index = itemData.index;
      if (index === undefined) {
        return;
      }

      if (this.historyData_.length > 0) {
        setTimeout(async () => {
          const item = await this.$.infiniteList.ensureItemRendered(
                           Math.min(this.historyData_.length - 1, index)) as
              HistoryItemElement;
          item.focusOnMenuButton();
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
    this.changeSelections_(indices, selected);
    this.lastSelectedIndex = index;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Template helpers:

  /**
   * Check whether the time difference between the given history item and the
   * next one is large enough for a spacer to be required.
   */
  protected needsTimeGap_(_item: HistoryEntry, index: number): boolean {
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
  protected isCardStart_(_item: HistoryEntry, i: number): boolean {
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
  protected isCardEnd_(_item: HistoryEntry, i: number): boolean {
    const length = this.historyData_.length;
    if (i === undefined || length === 0 || i > length - 1) {
      return false;
    }
    return i === length - 1 ||
        this.historyData_[i].dateRelativeDay !==
        this.historyData_[i + 1].dateRelativeDay;
  }

  protected hasResults_(): boolean {
    return this.historyData_.length > 0;
  }

  protected noResultsMessage_(): string {
    const messageId =
        this.searchedTerm !== '' ? 'noSearchResults' : 'noResults';
    return loadTimeData.getString(messageId);
  }

  protected canSearchMoreFromSite_(): boolean {
    return this.searchedTerm === '' ||
        this.searchedTerm !== this.actionMenuModel_?.item.domain;
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

  private getHistoryEmbeddingsMatches_(): HistoryEntry[] {
    return this.historyData_.slice(0, 3);
  }

  private showHistoryEmbeddings_(): boolean {
    return loadTimeData.getBoolean('enableHistoryEmbeddings') &&
        !!this.searchedTerm && this.historyData_?.length > 0;
  }

  private onIsActiveChanged_() {
    if (this.isActive) {
      // Active changed from false to true. Add the scroll observer.
      this.scrollTarget.addEventListener('scroll', this.scrollListener_);
    } else {
      // Active changed from true to false. Remove scroll observer.
      this.scrollTarget.removeEventListener('scroll', this.scrollListener_);
    }
  }

  private onScrollTargetChanged_(oldTarget?: HTMLElement) {
    if (oldTarget) {
      this.resizeObserver_.disconnect();
      oldTarget.removeEventListener('scroll', this.scrollListener_);
    }
    if (this.scrollTarget) {
      this.resizeObserver_.observe(this.scrollTarget);
      this.scrollTarget.addEventListener('scroll', this.scrollListener_);
      this.fillCurrentViewport();
    }
  }

  setScrollDebounceForTest(debounce: number) {
    this.scrollDebounce_ = debounce;
  }

  private onScrollOrResize_() {
    // Debounce by 200ms.
    if (this.scrollTimeout_) {
      clearTimeout(this.scrollTimeout_);
    }
    this.scrollTimeout_ =
        setTimeout(() => this.onScrollTimeout_(), this.scrollDebounce_);
  }

  private onScrollTimeout_() {
    this.scrollTimeout_ = null;
    const lowerScroll = this.scrollTarget.scrollHeight -
        this.scrollTarget.scrollTop - this.scrollTarget.offsetHeight;
    if (lowerScroll < 500) {
      this.onScrollToBottom_();
    }
    this.fire('scroll-timeout-for-test');
  }

  protected onLastFocusedChanged_(e: CustomEvent<{value: HTMLElement | null}>) {
    this.lastFocused_ = e.detail.value;
  }

  protected onListBlurredChanged_(e: CustomEvent<{value: boolean}>) {
    this.listBlurred_ = e.detail.value;
  }

  private getSelectedEntries_(): HistoryEntry[] {
    // `selectedItems` is a Set<number> of row-indexes.
    return Array.from(this.selectedItems, idx => this.historyData_[idx]);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-list': HistoryListElement;
  }
}

customElements.define(HistoryListElement.is, HistoryListElement);
