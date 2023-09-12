// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'infinite-list' is a component optimized for showing a list of
 * items that overflows the view and requires scrolling. For performance
 * reasons, the DOM items are incrementally added to the view as the user
 * scrolls through the list. The component expects a `max-height` property to be
 * specified in order to determine how many HTML elements to render initially.
 * The templates inside this element are used to create each list item's
 * HTML element. In order to associate the templates and items, a `data-type`
 * attribute is used. The `items` property specifies an array of list item data.
 * The component leverages an <iron-selector> to manage item selection and
 * styling. Items that should selectable must be associated with a template that
 * has a `data-selectable` attribute.
 */

import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {getDeepActiveElement} from 'chrome://resources/js/util_ts.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import {calculateSplices, PolymerElement, TemplateInstanceBase, templatize} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BiMap} from './bimap.js';
import {getTemplate} from './infinite_list.html.js';

export const NO_SELECTION: number = -1;

/**
 * HTML class name used to recognize selectable items.
 */
const SELECTABLE_CLASS_NAME: string = 'selectable';

export const selectorNavigationKeys: readonly string[] =
    Object.freeze(['ArrowUp', 'ArrowDown', 'Home', 'End']);

export interface InfiniteList {
  $: {
    selector: IronSelectorElement,
    container: HTMLElement,
  };
}

export class InfiniteList extends PolymerElement {
  static get is() {
    return 'infinite-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      maxHeight: {
        type: Number,
        observer: 'onMaxHeightChanged_',
      },

      items: {
        type: Array,
        observer: 'onItemsChanged_',
        value: [],
      },

      selectedItem: {
        type: Object,
        readonly: true,
        notify: true,
      },
    };
  }

  maxHeight: number;
  items: Object[];
  selectedItem: Object|null;
  private instanceConstructors_:
      Map<string,
          new(args: {item: Object, index?: number}) =>
              TemplateInstanceBase & HTMLElement>;
  private instances_: Array<TemplateInstanceBase&HTMLElement>;
  private selectableTypes_: Set<string>;
  private selectableIndexToItemIndex_: BiMap<number, number>|null;

  constructor() {
    super();

    /**
     * A map of type names associated with constructors used for creating list
     * item template instances.
     */
    this.instanceConstructors_ = new Map();

    /**
     * An array of template instances each of which contain the HTMLElement
     * associated with a given rendered item from the items array. The entries
     * are ordered to match the item's index.
     */
    this.instances_ = [];

    /**
     * A set of class names for which the selectable style class should be
     * applied.
     */
    this.selectableTypes_ = new Set();

    /**
     * Correlates the selectable item indexes to the `items` property indexes.
     */
    this.selectableIndexToItemIndex_ = null;
  }

  override ready() {
    super.ready();
    this.ensureTemplatized_();
    this.addEventListener('scroll', () => this.onScroll_());
  }

  /**
   * Create and insert as many DOM items as necessary to ensure all items are
   * rendered.
   */
  ensureAllDomItemsAvailable() {
    if (this.items.length > 0) {
      // Height may need to be updated when length has not changed, if previous
      // height calculation was performed when this element was not visible.
      const shouldUpdateHeight = this.instances_.length !== this.items.length ||
          this.$.container.style.height === '0px';
      for (let i = this.instances_.length; i < this.items.length; i++) {
        this.createAndInsertDomItem_(i);
      }

      if (shouldUpdateHeight) {
        this.updateHeight_();
      }
    }
  }

  scrollIndexIntoView(index: number) {
    assert(
        index >= 0 && index < this.selectableIndexToItemIndex_!.size(),
        'Index is out of range.');
    this.ensureSelectableDomItemAvailable_(index);
    this.getSelectableDomItem_(index)!.scrollIntoView(
        {behavior: 'smooth', block: 'nearest'});
  }

  /**
   * @param key Keyboard event key value.
   * @param focusItem Whether to focus the selected item.
   */
  navigate(key: string, focusItem?: boolean) {
    const selector = this.$.selector;

    if ((key === 'ArrowUp' && selector.selected === 0) || key === 'End') {
      this.ensureAllDomItemsAvailable();
      selector.selected = this.selectableIndexToItemIndex_!.size() - 1;
    } else {
      switch (key) {
        case 'ArrowUp':
          selector.selectPrevious();
          break;
        case 'ArrowDown':
          selector.selectNext();
          break;
        case 'Home':
          selector.selected = 0;
          break;
        case 'End':
          this.$.selector.selected =
              this.selectableIndexToItemIndex_!.size() - 1;
          break;
      }
    }

    if (focusItem) {
      (selector.selectedItem as HTMLElement).focus({preventScroll: true});
    }
  }

  private ensureTemplatized_() {
    // The user provided light-dom template(s) to use when stamping DOM items.
    const templates = this.querySelectorAll('template');
    assert(templates.length > 0, 'At least one template must be provided');

    // Initialize a map of class names to template instance constructors. On
    // inserting DOM nodes, a lookup will be performed against the map to
    // determine the correct constructor to use for rendering a given class
    // type.
    templates.forEach(template => {
      const type = template.dataset['type'];
      assert(type);
      const className = type;
      if (template.dataset['selectable'] !== undefined) {
        this.selectableTypes_.add(className);
      }

      const instanceProps = {
        item: true,
        // Selectable items require an `index` property to facilitate selection
        // and navigation capabilities exposed through the `selected` and
        // `selectedItem` properties, and the navigate method.
        index: this.selectableTypes_.has(className),
      };
      this.instanceConstructors_.set(
          className, templatize(template, this, {
                       parentModel: true,
                       instanceProps,
                     }) as {new (): TemplateInstanceBase & HTMLElement});
    });
  }

  /**
   * Create a DOM item and immediately insert it in the DOM tree. A reference is
   * stored in the instances_ array for future item lifecycle operations.
   */
  private createAndInsertDomItem_(index: number) {
    const instance = this.createItemInstance_(index);
    this.instances_[index] = instance;
    // Offset the insertion index to take into account the template elements
    // that are present in the light DOM.
    this.insertBefore(
        instance.root, this.children[index + this.instanceConstructors_.size]!);
  }

  private createItemInstance_(itemIndex: number): TemplateInstanceBase
      &HTMLElement {
    const item = this.items[itemIndex]!;
    const instanceConstructor =
        this.instanceConstructors_.get(item.constructor.name);
    assert(instanceConstructor);
    const itemSelectable = this.isItemSelectable_(item);
    const args = itemSelectable ?
        {item, index: this.selectableIndexToItemIndex_!.invGet(itemIndex)} :
        {item};
    const instance = new instanceConstructor(args);

    if (itemSelectable) {
      instance.children[0]!.classList.add(SELECTABLE_CLASS_NAME);
    }

    return instance;
  }

  /**
   * @return The average DOM item height.
   */
  private domItemAverageHeight_(): number {
    // It must always be true that if this logic is invoked, there should be
    // enough DOM items rendered to estimate an item average height. This is
    // ensured by the logic that observes the items array.
    const domItemCount = this.instances_.length;
    assert(domItemCount);
    const lastDomItem = this.lastElementChild as HTMLElement;
    return (lastDomItem.offsetTop + lastDomItem.offsetHeight) / domItemCount;
  }

  /**
   * Create and insert as many DOM items as necessary to ensure the selectable
   * item at the specified index is present.
   */
  private ensureSelectableDomItemAvailable_(selectableItemIndex: number) {
    const itemIndex =
        this.selectableIndexToItemIndex_!.get(selectableItemIndex)!;
    for (let i = this.instances_.length; i < itemIndex + 1; i++) {
      this.createAndInsertDomItem_(i);
    }
  }

  private getDomItem_(index: number): HTMLElement|undefined {
    const instance = this.instances_[index];
    return instance!.children[0] as HTMLElement;
  }

  private getSelectableDomItem_(selectableItemIndex: number): HTMLElement
      |undefined {
    return this.getDomItem_(
        this.selectableIndexToItemIndex_!.get(selectableItemIndex)!);
  }

  /**
   * @return The number of items required to fill the current viewport.
   */
  private viewportItemCount_(): number {
    return Math.ceil(this.maxHeight / this.domItemAverageHeight_());
  }

  /**
   * @return Whether DOM items were created or not.
   */
  private fillViewHeight_(height: number): boolean {
    const startTime = performance.now();

    // Ensure we have added enough DOM items so that we are able to estimate
    // item average height.
    assert(this.items.length);
    const initialDomItemCount = this.instances_.length;
    if (initialDomItemCount === 0) {
      this.createAndInsertDomItem_(0);
    }

    const desiredDomItemCount = Math.min(
        Math.ceil(height / this.domItemAverageHeight_()), this.items.length);
    // TODO(romanarora): Re-evaluate the average dom item height at given item
    // insertion counts in order to determine more precisely the right number of
    // items to render.
    for (let i = this.instances_.length; i < desiredDomItemCount; i++) {
      this.createAndInsertDomItem_(i);
    }

    // TODO(romanarora): Check if we have reached the desired height, and if not
    // keep adding items.

    if (initialDomItemCount !== desiredDomItemCount) {
      performance.mark(`tab_search:infinite_list_view_updated:${
          performance.now() - startTime}:metric_value`);

      return true;
    }

    return false;
  }

  private isItemSelectable_(item: Object): boolean {
    return this.selectableTypes_.has(item.constructor.name);
  }

  /**
   * @return Whether a list item is selected and focused.
   */
  private isItemSelectedAndFocused_(): boolean {
    const selectedItemIndex = this.$.selector.selected;
    if (selectedItemIndex !== undefined) {
      const selectedItem =
          this.getSelectableDomItem_(selectedItemIndex as number);
      if (selectedItem === undefined) {
        return false;
      }
      const deepActiveElement = getDeepActiveElement();

      return selectedItem === deepActiveElement ||
          (!!selectedItem.shadowRoot &&
           selectedItem.shadowRoot.activeElement === deepActiveElement);
    }

    return false;
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

    const selector = this.$.selector;
    if (selector.selected === undefined) {
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
  private onItemsChanged_(newItems: any[], oldItems: any[]) {
    if (this.instanceConstructors_.size === 0) {
      return;
    }

    if (newItems.length === 0) {
      this.selectableIndexToItemIndex_ = new BiMap();
      // If the new items array is empty, there is nothing to be rendered, so we
      // remove any DOM items present.
      this.removeDomItems_(0, this.instances_.length);
      this.resetSelected_();
    } else {
      const itemSelectedAndFocused = this.isItemSelectedAndFocused_();
      this.selectableIndexToItemIndex_ = new BiMap();
      newItems.forEach((item, index) => {
        if (this.isItemSelectable_(item)) {
          this.selectableIndexToItemIndex_!.set(
              this.selectableIndexToItemIndex_!.size(), index);
        }
      });

      // If we had previously rendered some DOM items, we perform a partial
      // update on them.
      if (oldItems.length !== 0) {
        // Update no more items than currently rendered and no less than what is
        // required to fill the viewport.
        const count =
            Math.max(this.instances_.length, this.viewportItemCount_());
        this.updateDomItems_(
            newItems.slice(0, count), oldItems.slice(0, count));
      }

      this.fillViewHeight_(this.scrollTop + this.maxHeight);

      // Since the new selectable items' length might be smaller than the old
      // selectable items' length, we need to check if the selected index is
      // still valid and if not adjust it.
      const selector = this.$.selector;
      if ((selector.selected as number) >=
          this.selectableIndexToItemIndex_!.size()) {
        selector.selected = this.selectableIndexToItemIndex_!.size() - 1;
      }

      // Restore focus to the selected item if necessary.
      if (itemSelectedAndFocused && selector.selected !== NO_SELECTION) {
        this.getSelectableDomItem_(selector.selected as number)!.focus();
      }
    }

    if (newItems.length !== oldItems.length) {
      this.updateHeight_();
    }

    this.dispatchEvent(
        new CustomEvent('viewport-filled', {bubbles: true, composed: true}));
  }

  private onMaxHeightChanged_(height: number) {
    this.style.maxHeight = height + 'px';
  }

  /**
   * Adds additional DOM items as needed to fill the view based on user scroll
   * interactions.
   */
  private onScroll_() {
    const scrollTop = this.scrollTop;
    if (scrollTop > 0 && this.instances_.length !== this.items.length) {
      if (this.fillViewHeight_(scrollTop + this.maxHeight)) {
        this.updateHeight_();
      }
    }
  }

  private updateDomItems_(newItems: any[], oldItems: any[]) {
    // Identify the differences between the original and new list of items.
    // These are represented as splice objects containing removed and added
    // item information at a given index. We leverage these splices to change
    // only the affected items.
    const splices = calculateSplices(newItems, oldItems);
    for (const splice of splices) {
      // If the splice applies to indices for which there are no instances yet
      // there is no need to update them yet.
      if (splice.index >= this.instances_.length) {
        continue;
      }

      if (splice.addedCount === splice.removed.length) {
        // If the number of added and removed items are equal, reuse the
        // existing DOM instances and simply update their item binding.
        const indexOfLastInstance =
            Math.min(splice.index + splice.addedCount, this.instances_.length);
        for (let i = splice.index; i < indexOfLastInstance; i++) {
          // If the types don't match, we need to replace the existing instance.
          if (oldItems[i].constructor !== newItems[i].constructor) {
            this.getDomItem_(i)!.remove();
            this.createAndInsertDomItem_(i);
            continue;
          }

          (this.instances_[i] as unknown as {item: any}).item = newItems[i];
        }
        continue;
      }

      // For simplicity, if new items have been added, we remove the no longer
      // accurate template instances following the splice index and allow the
      // component to ensure the viewport is full. If no items were added, we
      // simply remove the no longer existing items and update any following
      // template instances.
      // TODO(romanarora): Introduce a DOM item reuse pool for a more
      // efficient update.
      const removeCount = splice.addedCount !== 0 ?
          this.instances_.length - splice.index :
          splice.removed.length;
      this.removeDomItems_(splice.index, removeCount);
    }

    // Update the index property of the selectable item instances as it may no
    // longer be accurate after the splices have taken place.
    this.updateSelectableItemInstanceIndexes_();
  }

  private removeDomItems_(index: number, count: number) {
    this.instances_.splice(index, count).forEach(instance => {
      this.removeChild(instance.children[0]!);
    });
  }

  private updateSelectableItemInstanceIndexes_() {
    for (let itemIndex = 0; itemIndex < this.instances_.length; itemIndex++) {
      const selectableItemIndex =
          this.selectableIndexToItemIndex_!.invGet(itemIndex);
      if (selectableItemIndex !== undefined) {
        (this.instances_[itemIndex] as unknown as {index: number}).index =
            selectableItemIndex;
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
  private onSelectedChanged_() {
    const selector = this.$.selector;
    if (selector.selected === undefined) {
      return;
    }

    const selectedIndex = selector.selected;
    if (selectedIndex === 0) {
      this.scrollTo({top: 0, behavior: 'smooth'});
      return;
    }

    if (selectedIndex === this.selectableIndexToItemIndex_!.size() - 1) {
      this.getSelectableDomItem_(selectedIndex)!.scrollIntoView(
          {behavior: 'smooth'});
      return;
    }

    const previousItem =
        this.getSelectableDomItem_((selector.selected as number) - 1)!;
    if (previousItem.offsetTop < this.scrollTop) {
      previousItem.scrollIntoView({behavior: 'smooth', block: 'nearest'});
      return;
    }

    const nextItemIndex = (selector.selected as number) + 1;
    if (nextItemIndex < this.selectableIndexToItemIndex_!.size()) {
      this.ensureSelectableDomItemAvailable_(nextItemIndex);

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
  private resetSelected_() {
    this.$.selector.selected = undefined as unknown as string | number;
  }

  private selectableSelector_(): string {
    return '.' + SELECTABLE_CLASS_NAME;
  }

  set selected(index: number) {
    if (index === NO_SELECTION) {
      this.resetSelected_();
      return;
    }

    const selector = this.$.selector;
    if (index !== selector.selected) {
      assert(
          index < this.selectableIndexToItemIndex_!.size(),
          'Selection index is out of range.');
      this.ensureSelectableDomItemAvailable_(index);
      selector.selected = index;
    }
  }

  /** @return The selected index or -1 if none selected. */
  get selected(): number {
    return this.$.selector.selected !== undefined ?
        this.$.selector.selected as number :
        NO_SELECTION;
  }

  private onSelectedItemChanged_() {
    this.selectedItem = (this.$.selector.selectedItem as any)?.data;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'infinite-list': InfiniteList;
  }
}

customElements.define(InfiniteList.is, InfiniteList);
