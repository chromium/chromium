// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'infinite-list' is a component optimized for showing a list of
 * items that overflows the view and requires scrolling. For performance
 * reasons, the DOM items are incrementally added to the view as the user
 * scrolls through the list. The component expects a `max-height` property to be
 * specified in order to determine how many HTML elements to render initially.
 * Each list item's HTML element is creating using the template property, which
 * should be set to a function returning a TemplateResult corresponding to a
 * passed in list item and selection index. The `items` property specifies an
 * array of list item data. The component leverages CrSelectableMixin to manage
 * item selection. Selectable items should be designated as follows:
 * - The top level HTML element in the template result for such items should
 *   should have a "selectable" CSS class.
 * - The isSelectable property on infinite-list should be set to a function that
 *   returns whether an item passed to it is selectable.
 */

import {CrSelectableMixin} from 'chrome://resources/cr_elements/cr_selectable_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {CrLitElement, html, render} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues, TemplateResult} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './infinite_list.css.js';

export const NO_SELECTION: number = -1;

export const selectorNavigationKeys: readonly string[] =
    Object.freeze(['ArrowUp', 'ArrowDown', 'Home', 'End']);

const InfiniteListElementBase = CrSelectableMixin(CrLitElement);

export interface InfiniteList {
  $: {
    container: HTMLElement,
  };
}

export class InfiniteList<T = object> extends InfiniteListElementBase {
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
            .map(item => this.template(item)),
        this, {
          host: (this.getRootNode() as ShadowRoot).host,
        });

    // Render container + slot into shadow DOM
    return html`<div id="container" @keydown=${this.onKeyDown_}>
      <slot></slot>
    </div>`;
  }

  static override get properties() {
    return {
      maxHeight: {type: Number},
      numItemsDisplayed_: {type: Number},
      items: {type: Array},
      isSelectable: {type: Object},
      template: {type: Object},
    };
  }

  maxHeight?: number;
  items: T[] = [];
  template: (item: T) => TemplateResult = () => html``;
  // Overridden from CrSelectableMixin
  override selectable: string = '.selectable';
  isSelectable: (item: T) => boolean = (_item) => false;
  protected numItemsDisplayed_: number = 0;
  protected numItemsSelectable_: number = 0;

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.addEventListener('scroll', () => this.onScroll_());
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('maxHeight')) {
      this.style.maxHeight = `${this.maxHeight}px`;
    }

    if (changedProperties.has('items') || changedProperties.has('selectable')) {
      // Perform state updates.
      if (this.items.length === 0) {
        this.numItemsSelectable_ = 0;
        this.numItemsDisplayed_ = 0;
      } else {
        this.numItemsSelectable_ =
            this.items.filter(item => this.isSelectable(item)).length;
        this.numItemsDisplayed_ =
            Math.min(this.numItemsDisplayed_, this.items.length);
      }
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
      this.onSelectedChanged_();
    }
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
        index >= 0 && index < this.numItemsSelectable_,
        'Index is out of range.');
    await this.ensureSelectableDomItemAvailable_(index);
    this.getSelectableDomItem_(index)!.scrollIntoView(
        {behavior: 'smooth', block: 'nearest'});
  }

  /**
   * @param key Keyboard event key value.
   * @param focusItem Whether to focus the selected item.
   */
  async navigate(key: string, focusItem?: boolean) {
    if ((key === 'ArrowUp' && this.selected === 0) || key === 'End') {
      await this.ensureAllDomItemsAvailable();
      this.selected = this.numItemsSelectable_ - 1;
    } else {
      switch (key) {
        case 'ArrowUp':
          this.selectPrevious();
          break;
        case 'ArrowDown':
          this.selectNext();
          break;
        case 'Home':
          this.selected = 0;
          break;
        case 'End':
          this.selected = this.numItemsSelectable_ - 1;
          break;
      }
    }

    if (focusItem) {
      await this.updateComplete;
      (this.selectedItem as HTMLElement).focus({preventScroll: true});
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

  /**
   * Create and insert as many DOM items as necessary to ensure the selectable
   * item at the specified index is present.
   */
  private async ensureSelectableDomItemAvailable_(selectableItemIndex: number) {
    const additionalSelectableItemsToRender =
        selectableItemIndex + 1 - this.queryItems().length;
    if (additionalSelectableItemsToRender <= 0) {
      return;
    }

    let selectableItemsFound = 0;
    let itemIndex = this.numItemsDisplayed_;
    while (selectableItemsFound < additionalSelectableItemsToRender) {
      assert(itemIndex < this.items.length);
      if (this.isSelectable(this.items[itemIndex]!)) {
        selectableItemsFound++;
      }
      itemIndex++;
    }
    await this.updateNumItemsDisplayed_(itemIndex + 1);
  }

  private getSelectableDomItem_(selectableItemIndex: number): HTMLElement|null {
    const indexSelector = `[data-selection-index="${selectableItemIndex}"]`;
    return this.querySelector(
        `:scope > :is(${this.selectable})${indexSelector}`);
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
    this.numItemsDisplayed_ = itemsToDisplay;
    await this.updateComplete;
  }

  /**
   * @return Whether a list item is selected and focused.
   */
  private isItemSelectedAndFocused_(): boolean {
    if (!this.selectedItem) {
      return false;
    }
    const deepActiveElement = getDeepActiveElement();

    return this.selectedItem === deepActiveElement ||
        (!!this.selectedItem.shadowRoot &&
         this.selectedItem.shadowRoot.activeElement === deepActiveElement);
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
      if ((this.selected as number) >= this.numItemsSelectable_) {
        this.selected = this.numItemsSelectable_ - 1;
      }

      // Restore focus to the selected item if necessary.
      if (itemSelectedAndFocused && this.selected !== NO_SELECTION) {
        (this.selectedItem as HTMLElement).focus();
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
    if (selectedIndex === 0) {
      this.scrollTo({top: 0, behavior: 'smooth'});
      return;
    }

    if (selectedIndex === this.numItemsSelectable_ - 1) {
      this.selectedItem!.scrollIntoView({behavior: 'smooth'});
      return;
    }

    const previousItem =
        this.getSelectableDomItem_((this.selected as number) - 1)!;
    if (previousItem.offsetTop < this.scrollTop) {
      previousItem.scrollIntoView({behavior: 'smooth', block: 'nearest'});
      return;
    }

    const nextItemIndex = (this.selected as number) + 1;
    if (nextItemIndex < this.numItemsSelectable_) {
      await this.ensureSelectableDomItemAvailable_(nextItemIndex);

      const nextItem = this.getSelectableDomItem_(nextItemIndex)!;
      if (nextItem.offsetTop + nextItem.offsetHeight >
          this.scrollTop + this.offsetHeight) {
        nextItem.scrollIntoView({behavior: 'smooth', block: 'nearest'});
      }
    }
  }

  /**
   * Resets the selector's selection to the undefined state. This method
   * suppresses a closure validation that would require modifying the
   * IronSelectableBehavior's annotations for the selected property.
   */
  resetSelected() {
    this.selected = undefined as unknown as string | number;
  }

  async setSelected(index: number) {
    if (index === NO_SELECTION) {
      this.resetSelected();
      return;
    }

    if (index !== this.selected) {
      assert(
          index < this.numItemsSelectable_, 'Selection index is out of range.');
      await this.ensureSelectableDomItemAvailable_(index);
      this.selected = index;
    }
  }

  /** @return The selected index or -1 if none selected. */
  getSelected(): number {
    return this.selected !== undefined ? this.selected as number : NO_SELECTION;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'infinite-list': InfiniteList;
  }
}

customElements.define(InfiniteList.is, InfiniteList);
