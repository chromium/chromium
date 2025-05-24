// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_lazy_list/cr_lazy_list.js';
import '/strings.m.js';
import './item.js';
// <if expr="not is_chromeos">
import './promo_card.js';
// </if>

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrLazyListElement} from 'chrome://resources/cr_elements/cr_lazy_list/cr_lazy_list.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {deselectItems, selectAll, selectItem, updateAnchor} from './actions.js';
import {BookmarksCommandManagerElement} from './command_manager.js';
import {MenuSource} from './constants.js';
import type {BookmarksItemElement} from './item.js';
import {getCss} from './list.css.js';
import {getHtml} from './list.html.js';
import {StoreClientMixinLit} from './store_client_mixin_lit.js';
import type {BookmarksPageState, OpenCommandMenuDetail} from './types.js';
import {canReorderChildren, getDisplayedList} from './util.js';

const BookmarksListElementBase = StoreClientMixinLit(CrLitElement);

export interface BookmarksListElement {
  $: {
    list: CrLazyListElement,
    message: HTMLElement,
  };
}

export class BookmarksListElement extends BookmarksListElementBase {
  static get is() {
    return 'bookmarks-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      displayedIds_: {type: Array},
      searchTerm_: {type: String},
      selectedFolder_: {type: String},
      selectedItems_: {type: Object},
      focusedIndex_: {type: Number},
      shouldShowPromoCard_: {type: Boolean},
    };
  }

  protected accessor displayedIds_: string[] = [];
  private accessor focusedIndex_: number = 0;
  private eventTracker_: EventTracker = new EventTracker();
  private accessor searchTerm_: string = '';
  protected accessor selectedFolder_: string = '';
  private accessor selectedItems_: Set<string> = new Set();
  protected accessor shouldShowPromoCard_: boolean = false;

  override firstUpdated() {
    this.addEventListener('click', () => this.deselectItems_());
    this.addEventListener('contextmenu', e => this.onContextMenu_(e));
    this.addEventListener(
        'open-command-menu',
        e => this.onOpenCommandMenu_(e as CustomEvent<OpenCommandMenuDetail>));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('displayedIds_')) {
      // Reset the focused index if it's out of bounds for the new array value.
      if (this.focusedIndex_ > this.displayedIds_.length - 1) {
        this.focusedIndex_ = 0;
      }
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('searchTerm_') ||
        changedPrivateProperties.has('selectedFolder_')) {
      this.scrollTop = 0;
    }

    if (changedPrivateProperties.has('displayedIds_')) {
      // Get the last selection from the previous value of selectedItems_ if
      // selectedItems_ also changed, and was not previously undefined (as
      // is the case at initialization). Otherwise, get the last selection from
      // the current value of selectedItems_, since it is the same as the prior
      // value.
      let lastSelectedItems: Set<string> = this.selectedItems_;
      if (changedPrivateProperties.has('selectedItems_') &&
          changedPrivateProperties.get('selectedItems_') !== undefined) {
        lastSelectedItems =
            changedPrivateProperties.get('selectedItems_') as Set<string>;
      }
      const lastSelection =
          lastSelectedItems.size > 0 ? Array.from(lastSelectedItems)[0]! : null;
      this.onDisplayedIdsChanged_(
          changedPrivateProperties.get('displayedIds_') as string[],
          lastSelection);
    }
  }

  override connectedCallback() {
    super.connectedCallback();
    this.updateFromStore();

    this.eventTracker_.add(
        document, 'highlight-items',
        (e: Event) => this.onHighlightItems_(e as CustomEvent<string[]>));
    this.eventTracker_.add(
        document, 'import-began', () => this.onImportBegan_());
    this.eventTracker_.add(
        document, 'import-ended', () => this.onImportEnded_());
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.eventTracker_.remove(document, 'highlight-items');
  }

  override onStateChanged(state: BookmarksPageState) {
    this.displayedIds_ = getDisplayedList(state);
    this.searchTerm_ = state.search.term;
    this.selectedFolder_ = state.selectedFolder;
    this.selectedItems_ = state.selection.items;
  }

  getDropTarget(): HTMLElement {
    return this.$.message;
  }

  private onDisplayedIdsChanged_(
      previous: string[], lastSelected: string|null) {
    // Clear any previous 'items-rendered' listener when the list changes, as
    // the |selectIndex| from last time may no longer be valid.
    this.eventTracker_.remove(this.$.list, 'items-rendered');

    let selectIndex = -1;
    let skipFocus = false;
    if (this.matches(':focus-within')) {
      if (lastSelected !== null) {
        skipFocus = this.displayedIds_.some(id => lastSelected === id);
        selectIndex =
            previous ? previous.findIndex(id => lastSelected === id) : -1;
      }
      if (selectIndex === -1 && this.displayedIds_.length > 0) {
        selectIndex = 0;
      } else {
        selectIndex = Math.min(selectIndex, this.displayedIds_.length - 1);
      }
    }

    if (selectIndex > -1) {
      if (skipFocus) {
        // Mimic iron-list by blurring the item in this case.
        const active = this.shadowRoot.activeElement;
        if (active) {
          (active as HTMLElement).blur();
        }
      } else {
        this.eventTracker_.add(
            this.$.list, 'items-rendered',
            () => this.focusMenuButton_(selectIndex));
      }
    }

    PluralStringProxyImpl.getInstance()
        .getPluralString('listChanged', this.displayedIds_.length)
        .then(label => {
          getAnnouncerInstance().announce(label);
        });
  }

  private async focusMenuButton_(index: number) {
    const element =
        await this.$.list.ensureItemRendered(index) as BookmarksItemElement;
    element.focusMenuButton();
  }

  /**
   * Scroll the list so that |itemId| is visible, if it is not already.
   */
  private scrollToId_(itemId: string) {
    const index = this.displayedIds_.indexOf(itemId);
    const list = this.$.list;
    if (index >= 0 && index < list.domItems().length) {
      (list.domItems()[index] as HTMLElement).scrollIntoViewIfNeeded();
    }
  }

  protected emptyListMessage_(): string {
    let emptyListMessage = 'noSearchResults';
    if (!this.searchTerm_) {
      emptyListMessage =
          canReorderChildren(this.getState(), this.getState().selectedFolder) ?
          'emptyList' :
          'emptyUnmodifiableList';
    }
    return loadTimeData.getString(emptyListMessage);
  }

  protected isEmptyList_(): boolean {
    return this.displayedIds_.length === 0;
  }

  private deselectItems_() {
    this.dispatch(deselectItems());
  }

  private onOpenCommandMenu_(e: CustomEvent<{source: MenuSource}>) {
    // If the item is not visible, scroll to it before rendering the menu.
    if (e.detail.source === MenuSource.ITEM) {
      this.scrollToId_((e.composedPath()[0] as BookmarksItemElement).itemId);
    }
  }

  /**
   * Highlight a list of items by selecting them, scrolling them into view and
   * focusing the first item.
   */
  private async onHighlightItems_(e: CustomEvent<string[]>) {
    // Ensure that we only select items which are actually being displayed.
    // This should only matter if an unrelated update to the bookmark model
    // happens with the perfect timing to end up in a tracked batch update.
    const toHighlight =
        e.detail.filter((item) => this.displayedIds_.indexOf(item) !== -1);

    if (toHighlight.length <= 0) {
      return;
    }

    const leadId = toHighlight[0]!;
    this.dispatch(selectAll(toHighlight, this.getState(), leadId));

    // Wait for the change to selectedItems_ to reflect in the DOM.
    await this.updateComplete;
    const leadIndex = this.displayedIds_.indexOf(leadId);
    assert(leadIndex !== -1);
    // Ensure the new list addition has been rendered by cr-lazy-list.
    const element = await this.$.list.ensureItemRendered(leadIndex);
    element.scrollIntoViewIfNeeded();
    element.focus();
  }

  private onImportBegan_() {
    getAnnouncerInstance().announce(loadTimeData.getString('importBegan'));
  }

  private onImportEnded_() {
    getAnnouncerInstance().announce(loadTimeData.getString('importEnded'));
  }

  protected onItemKeydown_(e: KeyboardEvent) {
    let handled = true;
    let focusMoved = false;
    let focusedIndex = Number((e.target as HTMLElement).dataset['index']);
    const oldFocusedIndex = focusedIndex;
    const cursorModifier = isMac ? e.metaKey : e.ctrlKey;
    if (e.key === 'ArrowUp') {
      focusedIndex--;
      focusMoved = true;
    } else if (e.key === 'ArrowDown') {
      focusedIndex++;
      focusMoved = true;
      e.preventDefault();
    } else if (e.key === 'Home') {
      focusedIndex = 0;
      focusMoved = true;
    } else if (e.key === 'End') {
      focusedIndex = this.displayedIds_.length - 1;
      focusMoved = true;
    } else if (e.key === ' ' && cursorModifier) {
      this.dispatch(
          selectItem(this.displayedIds_[focusedIndex]!, this.getState(), {
            clear: false,
            range: false,
            toggle: true,
          }));
    } else {
      handled = false;
    }

    if (focusMoved) {
      // Focus only moves if the key is an arrow key or page up/down, which
      // means we've handled the key event.
      focusedIndex =
          Math.min(this.displayedIds_.length - 1, Math.max(0, focusedIndex));
      this.focusedIndex_ = focusedIndex;
      assert(handled);
      e.stopPropagation();

      // Once the item is rendered, update the anchor and config if needed.
      this.updateAnchorsAndSelectionForFocusChange_(
          e.shiftKey, cursorModifier, oldFocusedIndex, focusedIndex);
      return;
    }

    // Prevent the cr-lazy-list from changing focus on enter.
    if (e.key === 'Enter') {
      if ((e.composedPath()[0] as HTMLElement).tagName === 'CR-ICON-BUTTON') {
        return;
      }
      if (e.composedPath()[0] instanceof HTMLButtonElement) {
        handled = true;
      }
    }

    if (!handled) {
      handled = BookmarksCommandManagerElement.getInstance().handleKeyEvent(
          e, this.getState().selection.items);
    }

    if (handled) {
      e.stopPropagation();
    }
  }

  private async updateAnchorsAndSelectionForFocusChange_(
      shiftKey: boolean, cursorModifier: boolean, oldFocusedIndex: number,
      focusedIndex: number) {
    const element = await this.$.list.ensureItemRendered(focusedIndex);
    element.focus();

    if (cursorModifier && !shiftKey) {
      this.dispatch(updateAnchor(this.displayedIds_[focusedIndex]!));
    } else {
      // If shift-selecting with no anchor, use the old focus index.
      if (shiftKey && this.getState().selection.anchor === null) {
        this.dispatch(updateAnchor(this.displayedIds_[oldFocusedIndex]!));
      }
      // If the focus moved from something other than a Ctrl + move event,
      // update the selection.
      const config = {
        clear: !cursorModifier,
        range: shiftKey,
        toggle: false,
      };

      this.dispatch(selectItem(
          this.displayedIds_[focusedIndex]!, this.getState(), config));
    }
  }

  private onContextMenu_(e: MouseEvent) {
    e.preventDefault();
    this.deselectItems_();

    this.dispatchEvent(new CustomEvent('open-command-menu', {
      bubbles: true,
      composed: true,
      detail: {
        x: e.clientX,
        y: e.clientY,
        source: MenuSource.LIST,
      },
    }));
  }

  protected onItemFocus_(e: Event) {
    const renderedItems = this.$.list.domItems();
    const focusedIdx = Array.from(renderedItems).findIndex(item => {
      return item === e.target || item.shadowRoot?.activeElement === e.target;
    });

    if (focusedIdx !== -1) {
      this.focusedIndex_ = focusedIdx;
    }
  }

  protected getAriaRowindex_(index: number): number {
    return index + 1;
  }

  protected getTabindex_(index: number): number {
    return index === this.focusedIndex_ ? 0 : -1;
  }

  protected getAriaSelected_(id: string): boolean {
    return this.selectedItems_.has(id);
  }

  protected updateShouldShowPromoCard_(e: Event) {
    this.shouldShowPromoCard_ = (e as CustomEvent).detail.shouldShowPromoCard;
  }

  setDisplayedIdsForTesting(ids: string[]) {
    this.displayedIds_ = ids;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'bookmarks-list': BookmarksListElement;
  }
}

customElements.define(BookmarksListElement.is, BookmarksListElement);
