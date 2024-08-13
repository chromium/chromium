// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'infinite-list' is a component optimized for showing a list of
 * items that overflows the view and requires scrolling. For performance
 * reasons, the DOM items are incrementally added to the view as the user
 * scrolls through the list.
 * The component expects a `max-height` property to be specified in order to
 * determine how many HTML elements to render initially.
 * Each list item's HTML element is creating using the `template` property,
 * which should be set to a function returning a TemplateResult corresponding
 * to a passed in list item and index in the list.
 * The `items` property specifies an array of list item data.
 * The `isSelectable()` property should return true when a selectable list
 * item from `items` is passed in, and false otherwise. It defaults to returning
 * true for all items. This is used for navigating through the list in response
 * to key presses as non-selectable items are treated as non-navigable. Note
 * that the list assumes the majority of items are selectable/navigable (as is
 * the case in tab search). Passing in a list with a very large number of
 * non-selectable items may result in reduced performance when navigating.
 * The `selected` property is the index of the selected item in the list, or
 * NO_SELECTION if nothing is selected.
 */

import {assert} from 'chrome://resources/js/assert.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {CrLitElement, html, render} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues, TemplateResult} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './infinite_list.css.js';

export const NO_SELECTION: number = -1;

export const selectorNavigationKeys: readonly string[] =
    Object.freeze(['ArrowUp', 'ArrowDown', 'Home', 'End']);

export interface InfiniteList {
  $: {
    container: HTMLElement,
    slot: HTMLSlotElement,
  };
}

export class InfiniteList<T = object> extends CrLitElement {
  static get is() {
    return 'infinite-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    // Render items into light DOM using the client provided template
    render(
        this.items.slice(0, this.numItemsDisplayed_)
            .map((item, index) => this.template(item, index)),
        this, {
          host: (this.getRootNode() as ShadowRoot).host,
        });

    // Render container + slot into shadow DOM
    return html`<div id="container" @keydown=${this.onKeyDown_}>
      <slot id="slot" @slotchange=${this.onSlotChange_}></slot>
    </div>`;
  }

  static override get properties() {
    return {
      maxHeight: {type: Number},
      numItemsDisplayed_: {type: Number},
      items: {type: Array},
      isSelectable: {type: Object},
      selected: {type: Number},
      template: {type: Object},
    };
  }

  maxHeight?: number;
  items: T[] = [];
  template: (item: T, index: number) => TemplateResult = () => html``;
  selected: number = NO_SELECTION;
  isSelectable: (item: T) => boolean = (_item) => true;
  protected numItemsDisplayed_: number = 0;
  private selectedItem_: Element|null = null;
  private firstSelectableIndex_: number = NO_SELECTION;
  private lastSelectableIndex_: number = NO_SELECTION;

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.addEventListener('scroll', () => this.onScroll_());
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('maxHeight')) {
      this.style.maxHeight = `${this.maxHeight}px`;
    }

    if (changedProperties.has('items') ||
        changedProperties.has('isSelectable')) {
      // Perform state updates.
      if (this.items.length === 0) {
        this.numItemsDisplayed_ = 0;
      } else {
        this.numItemsDisplayed_ =
            Math.min(this.numItemsDisplayed_, this.items.length);
      }
      this.firstSelectableIndex_ = this.getNextSelectableIndex_(-1);
      this.lastSelectableIndex_ =
          this.getPreviousSelectableIndex_(this.items.length);
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('items') || changedProperties.has('maxHeight')) {
      const previous = changedProperties.get('items');
      if (previous !== undefined || this.items.length !== 0) {
        this.onItemsChanged_(previous ? (previous as T[]).length : 0);
      }
    }

    if (changedProperties.has('selected')) {
      this.updateSelectedItem_();
      this.onSelectedChanged_();
    }
  }

  private getNextSelectableIndex_(index: number): number {
    const increment =
        this.items.slice(index + 1).findIndex(item => this.isSelectable(item));
    return increment === -1 ? NO_SELECTION : index + 1 + increment;
  }

  private getPreviousSelectableIndex_(index: number): number {
    return index < 0 ? NO_SELECTION :
                       this.items.slice(0, index).findLastIndex(
                           item => this.isSelectable(item));
  }

  private onSlotChange_() {
    this.updateSelectedItem_();
    this.fire('rendered-items-changed');
  }

  private updateSelectedItem_() {
    if (!this.items) {
      return;
    }

    const domItem = this.selected === NO_SELECTION ?
        null :
        this.$.slot.assignedElements()[this.selected] || null;
    if (domItem === this.selectedItem_) {
      return;
    }

    if (this.selectedItem_ !== null) {
      this.selectedItem_.classList.toggle('selected', false);
    }

    if (domItem !== null) {
      domItem.classList.toggle('selected', true);
    }

    this.selectedItem_ = domItem;
    this.fire('selected-change', {item: this.selectedItem_});
  }

  get selectedItem(): Element|null {
    return this.selectedItem_;
  }

  /**
   * Create and insert as many DOM items as necessary to ensure all items are
   * rendered.
   */
  async ensureAllDomItemsAvailable() {
    if (this.items.length === 0) {
      return;
    }

    // Height may need to be updated when length has not changed, if previous
    // height calculation was performed when this element was not visible.
    const shouldUpdateHeight = this.numItemsDisplayed_ !== this.items.length ||
        this.$.container.style.height === '0px';
    if (this.numItemsDisplayed_ !== this.items.length) {
      await this.updateNumItemsDisplayed_(this.items.length);
    }

    if (shouldUpdateHeight) {
      this.updateHeight_();
    }
  }

  async scrollIndexIntoView(index: number) {
    assert(
        index >= this.firstSelectableIndex_ &&
            index <= this.lastSelectableIndex_,
        'Index is out of range.');
    await this.updateNumItemsDisplayed_(index + 1);
    this.getDomItem_(index)!.scrollIntoView(
        {behavior: 'smooth', block: 'nearest'});
  }

  /**
   * @param key Keyboard event key value.
   * @param focusItem Whether to focus the selected item.
   */
  async navigate(key: string, focusItem?: boolean) {
    if ((key === 'ArrowUp' && this.selected === this.firstSelectableIndex_) ||
        key === 'End') {
      await this.ensureAllDomItemsAvailable();
      this.selected = this.lastSelectableIndex_;
    } else {
      switch (key) {
        case 'ArrowUp':
          this.selected = this.getPreviousSelectableIndex_(this.selected);
          break;
        case 'ArrowDown':
          const next = this.getNextSelectableIndex_(this.selected);
          this.selected =
              next === NO_SELECTION ? this.getNextSelectableIndex_(-1) : next;
          break;
        case 'Home':
          this.selected = this.firstSelectableIndex_;
          break;
        case 'End':
          this.selected = this.lastSelectableIndex_;
          break;
      }
    }

    if (focusItem) {
      await this.updateComplete;
      (this.selectedItem_ as HTMLElement).focus({preventScroll: true});
    }
  }

  /**
   * @return The average DOM item height.
   */
  private domItemAverageHeight_(): number {
    // This logic should only be invoked if the list is non-empty and at least
    // one DOM item has been rendered so that an item average height can be
    // estimated. This is ensured by the callers.
    assert(this.items.length > 0);
    const slot = this.shadowRoot!.querySelector('slot');
    assert(slot);
    const domItems = slot.assignedElements();
    assert(domItems.length > 0);
    const lastDomItem = domItems.at(-1) as HTMLElement;
    return (lastDomItem.offsetTop + lastDomItem.offsetHeight) / domItems.length;
  }

  private getDomItem_(index: number): Element|null {
    return this.$.slot.assignedElements()[index] || null;
  }

  async fillCurrentViewHeight() {
    if (this.numItemsDisplayed_ === 0 || !this.maxHeight) {
      return;
    }

    // Update height if new items are added or previous height calculation was
    // performed when this element was not visible.
    const added = await this.fillViewHeight_(this.maxHeight + this.scrollTop);
    if (added || this.$.container.style.height === '0px') {
      this.updateHeight_();
    }
  }

  /**
   * @return Whether DOM items were created or not.
   */
  private async fillViewHeight_(height: number): Promise<boolean> {
    const startTime = performance.now();

    // Ensure we have added enough DOM items so that we are able to estimate
    // item average height.
    assert(this.items.length);
    const initialDomItemCount = this.numItemsDisplayed_;
    if (this.numItemsDisplayed_ === 0) {
      await this.updateNumItemsDisplayed_(1);
    }

    const itemHeight = this.domItemAverageHeight_();
    // If this happens, the math below will be incorrect and we will render
    // all items. So return early.
    if (itemHeight === 0) {
      return false;
    }

    const desiredDomItemCount =
        Math.min(Math.ceil(height / itemHeight), this.items.length);
    if (desiredDomItemCount > this.numItemsDisplayed_) {
      await this.updateNumItemsDisplayed_(desiredDomItemCount);
    }

    // TODO(romanarora): Re-evaluate the average dom item height at given item
    // insertion counts in order to determine more precisely the right number of
    // items to render.

    // TODO(romanarora): Check if we have reached the desired height, and if not
    // keep adding items.

    if (initialDomItemCount !== desiredDomItemCount) {
      performance.mark(`tab_search:infinite_list_view_updated:${
          performance.now() - startTime}:metric_value`);
      return true;
    }

    return false;
  }

  private async updateNumItemsDisplayed_(itemsToDisplay: number) {
    if (itemsToDisplay <= this.numItemsDisplayed_) {
      return;
    }

    this.numItemsDisplayed_ = itemsToDisplay;
    await this.updateComplete;
  }

  /**
   * @return Whether a list item is selected and focused.
   */
  private isItemSelectedAndFocused_(): boolean {
    if (!this.selectedItem_) {
      return false;
    }
    const deepActiveElement = getDeepActiveElement();

    return this.selectedItem_ === deepActiveElement ||
        (!!this.selectedItem_.shadowRoot &&
         this.selectedItem_.shadowRoot.activeElement === deepActiveElement);
  }

  /**
   * Handles key events when list item elements have focus.
   */
  protected onKeyDown_(e: KeyboardEvent) {
    // Do not interfere with any parent component that manages 'shift' related
    // key events.
    if (e.shiftKey) {
      return;
    }

    if (this.selected === undefined) {
      // No tabs matching the search text criteria.
      return;
    }

    if (selectorNavigationKeys.includes(e.key)) {
      this.navigate(e.key, true);
      e.stopPropagation();
      e.preventDefault();
    }
  }

  /**
   * Ensures that when the items property changes, only a chunk of the items
   * needed to fill the current scroll position view are added to the DOM, thus
   * improving rendering performance.
   */
  private async onItemsChanged_(previousLength: number) {
    if (this.items.length === 0) {
      this.resetSelected();
    } else {
      if (this.maxHeight === undefined || this.maxHeight === 0) {
        return;
      }
      const itemSelectedAndFocused = this.isItemSelectedAndFocused_();
      await this.fillViewHeight_(this.scrollTop + this.maxHeight);

      // Since the new selectable items' length might be smaller than the old
      // selectable items' length, we need to check if the selected index is
      // still valid and if not adjust it.
      if ((this.selected as number) >= this.lastSelectableIndex_) {
        this.selected = this.lastSelectableIndex_;
      }

      // Restore focus to the selected item if necessary.
      if (itemSelectedAndFocused && this.selected !== NO_SELECTION) {
        (this.selectedItem_ as HTMLElement).focus();
      }
    }

    if (this.items.length !== previousLength) {
      this.updateHeight_();
    }

    this.fire('viewport-filled');
  }

  /**
   * Adds additional DOM items as needed to fill the view based on user scroll
   * interactions.
   */
  private async onScroll_() {
    const scrollTop = this.scrollTop;
    if (scrollTop > 0 && this.numItemsDisplayed_ !== this.items.length) {
      if (this.maxHeight === undefined) {
        return;
      }
      const added = await this.fillViewHeight_(scrollTop + this.maxHeight);
      if (added) {
        this.updateHeight_();
      }
    }
  }

  /**
   * Sets the height of the component based on an estimated average DOM item
   * height and the total number of items.
   */
  private updateHeight_() {
    const estScrollHeight = this.items.length > 0 ?
        this.items.length * this.domItemAverageHeight_() :
        0;
    this.$.container.style.height = estScrollHeight + 'px';
  }

  /**
   * Ensure the scroll view can fully display a preceding or following list item
   * to the one selected, if existing.
   *
   * TODO(romanarora): Selection navigation behavior should be configurable. The
   * approach followed below might not be desired by all component users.
   */
  protected async onSelectedChanged_() {
    if (this.selected === undefined) {
      return;
    }

    const selectedIndex = this.selected;
    if (selectedIndex === this.firstSelectableIndex_) {
      this.scrollTo({top: 0, behavior: 'smooth'});
      return;
    }

    if (selectedIndex === this.lastSelectableIndex_) {
      this.selectedItem_!.scrollIntoView({behavior: 'smooth'});
      return;
    }

    const previousIndex = this.getPreviousSelectableIndex_(this.selected);
    const previousItem = previousIndex === NO_SELECTION ?
        null :
        this.getDomItem_(previousIndex) as HTMLElement;
    if (!!previousItem && (previousItem.offsetTop < this.scrollTop)) {
      previousItem.scrollIntoView({behavior: 'smooth', block: 'nearest'});
      return;
    }

    const nextItemIndex = this.getNextSelectableIndex_(this.selected);
    if (nextItemIndex !== NO_SELECTION) {
      await this.updateNumItemsDisplayed_(nextItemIndex + 1);

      const nextItem = this.getDomItem_(nextItemIndex) as HTMLElement;
      if (nextItem.offsetTop + nextItem.offsetHeight >
          this.scrollTop + this.offsetHeight) {
        nextItem.scrollIntoView({behavior: 'smooth', block: 'nearest'});
      }
    }
  }

  resetSelected() {
    this.selected = NO_SELECTION;
  }

  async setSelected(index: number) {
    if (index === NO_SELECTION) {
      this.resetSelected();
      return;
    }

    if (index !== this.selected) {
      assert(
          index <= this.lastSelectableIndex_,
          'Selection index is out of range.');
      await this.updateNumItemsDisplayed_(index + 1);
      this.selected = index;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'infinite-list': InfiniteList;
  }
}

customElements.define(InfiniteList.is, InfiniteList);
