// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'selectable-lazy-list' is a wrapper around `lazy-list` that
 * provides list selection and navigation as required by the Tab Search UI.
 * The component expects a `max-height` property to be specified in order to
 * determine how many HTML elements to render initially.
 * The `items`, `itemSize` and `template` properties are passed through to the
 * inner `cr-lazy-list'.
 * `expandedList` is an attribute for showing an extra 16px padding at the
 * bottom of the innner list (tab search desired styling).
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

import 'chrome://resources/cr_elements/cr_lazy_list/cr_lazy_list.js';

import type {CrLazyListElement} from 'chrome://resources/cr_elements/cr_lazy_list/cr_lazy_list.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement, html, render} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues, TemplateResult} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './selectable_lazy_list.css.js';

export const NO_SELECTION: number = -1;

export const selectorNavigationKeys: readonly string[] =
    Object.freeze(['ArrowUp', 'ArrowDown', 'Home', 'End']);

export class SelectableLazyListElement<T = object> extends CrLitElement {
  static get is() {
    return 'selectable-lazy-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    // Render items into light DOM using the client provided template
    render(
        html`<cr-lazy-list id="list" .scrollTarget="${this}"
          .listItemHost="${(this.getRootNode() as ShadowRoot).host}"
          .itemSize="${this.itemSize}" .items="${this.items}"
          .minViewportHeight="${this.maxHeight}"
          .template="${this.template}"
          .restoreFocusElement="${this.selectedItem_}"
          .style="${this.getListPaddingStyle_()}"
          @keydown="${this.onKeyDown_}"
          @viewport-filled="${this.updateSelectedItem_}"
          @fill-height-start="${this.onFillHeightStart_}"
          @fill-height-end="${this.onFillHeightEnd_}">
        </cr-lazy-list>`,
        this, {
          host: this,
        });
    return html`<slot></slot>`;
  }

  static override get properties() {
    return {
      expandedList: {type: Boolean},
      maxHeight: {type: Number},
      items: {type: Array},
      itemSize: {type: Number},
      isSelectable: {type: Object},
      selected: {type: Number},
      template: {type: Object},
      selectedItem_: {type: Object},
    };
  }

  expandedList: boolean = false;
  maxHeight?: number;
  items: T[] = [];
  itemSize: number = 100;
  template: (item: T, index: number) => TemplateResult = () => html``;
  selected: number = NO_SELECTION;
  isSelectable: (item: T) => boolean = (_item) => true;
  private selectedItem_: Element|null = null;
  private firstSelectableIndex_: number = NO_SELECTION;
  private lastSelectableIndex_: number = NO_SELECTION;
  private viewportFillStartTime_: number = 0;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('items') ||
        changedProperties.has('isSelectable')) {
      // Perform selection state updates.
      if (this.items.length === 0) {
        this.resetSelected();
      }
      this.firstSelectableIndex_ = this.getNextSelectableIndex_(-1);
      this.lastSelectableIndex_ =
          this.getPreviousSelectableIndex_(this.items.length);
      if (this.selected > this.lastSelectableIndex_) {
        this.selected = this.lastSelectableIndex_;
      }
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('maxHeight') && this.maxHeight !== 0) {
      this.style.maxHeight = `${this.maxHeight}px`;
    }

    if (changedProperties.has('selected')) {
      this.updateSelectedItem_();
      this.onSelectedChanged_();
    }
  }

  private getListPaddingStyle_(): string {
    return this.expandedList ? 'padding-bottom: 16px' : '';
  }

  // Utilities
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

  private getDomItem_(index: number): HTMLElement|null {
    return this.querySelector<HTMLElement>(
        `cr-lazy-list > *:nth-child(${index + 1})`);
  }

  private lazyList_(): CrLazyListElement {
    const list = this.querySelector('cr-lazy-list');
    assert(list);
    return list;
  }

  private updateSelectedItem_() {
    if (!this.items) {
      return;
    }

    const domItem =
        this.selected === NO_SELECTION ? null : this.getDomItem_(this.selected);
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

  // Public methods
  get selectedItem(): Element|null {
    return this.selectedItem_;
  }

  fillCurrentViewport(): Promise<void> {
    return this.lazyList_().fillCurrentViewport();
  }

  /**
   * Create and insert as many DOM items as necessary to ensure all items are
   * rendered.
   */
  async ensureAllDomItemsAvailable() {
    await this.lazyList_().ensureItemRendered(this.items.length - 1);
  }

  async scrollIndexIntoView(index: number) {
    assert(
        index >= this.firstSelectableIndex_ &&
            index <= this.lastSelectableIndex_,
        'Index is out of range.');
    const newItem = await this.lazyList_().ensureItemRendered(index);
    newItem.scrollIntoView({behavior: 'smooth', block: 'nearest'});
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

  // Event handlers
  private onFillHeightStart_() {
    this.viewportFillStartTime_ = performance.now();
  }

  private onFillHeightEnd_() {
    performance.mark(`tab_search:infinite_list_view_updated:${
        performance.now() - this.viewportFillStartTime_}:metric_value`);
  }

  /**
   * Handles key events when list item elements have focus.
   */
  private onKeyDown_(e: KeyboardEvent) {
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
   * Ensure the scroll view can fully display a preceding or following list item
   * to the one selected, if existing.
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
    const previousItem =
        previousIndex === NO_SELECTION ? null : this.getDomItem_(previousIndex);
    if (!!previousItem && (previousItem.offsetTop < this.scrollTop)) {
      previousItem.scrollIntoView({behavior: 'smooth', block: 'nearest'});
      return;
    }

    const nextItemIndex = this.getNextSelectableIndex_(this.selected);
    if (nextItemIndex !== NO_SELECTION) {
      const nextItem = await this.lazyList_().ensureItemRendered(nextItemIndex);
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
      await this.lazyList_().ensureItemRendered(index);
      this.selected = index;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'selectable-lazy-list': SelectableLazyListElement;
  }
}

customElements.define(SelectableLazyListElement.is, SelectableLazyListElement);
