// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'infinite-list' is a component optimized for showing a list of
 * items that overflows the view and requires scrolling. For performance
 * reasons, the DOM items are added incrementally to the view as the user
 * scrolls through the list. The template inside this element is used to create
 * each list item's HTML element. The `items` property specifies an array of
 * list item data. The component leverages an <iron-selector> to manage item
 * selection and styling.
 */

import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {calculateSplices, html, PolymerElement, TemplateInstanceBase, templatize} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @type {number} */
export const NO_SELECTION = -1;

/** @type {!Array<string>} */
export const selectorNavigationKeys =
    Object.freeze(['ArrowUp', 'ArrowDown', 'Home', 'End']);

export class InfiniteList extends PolymerElement {
  static get is() {
    return 'infinite-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {number} */
      maxHeight: {
        type: Number,
        observer: 'onMaxHeightChanged_',
      },

      /** @type {!Array<!Object>} */
      items: {
        type: Array,
        observer: 'onItemsChanged_',
        value: [],
      },
    };
  }

  constructor() {
    super();

    /**
     * A constructor used for creating list item template instances.
     * @private {?function(new:TemplateInstanceBase, !Object)}
     */
    this.instanceConstructor_ = null;

    /**
     * An array of template instances each of which contain the HTMLElement
     * associated with a given rendered item from the items array. The entries
     * are ordered to match the item's index.
     * @private {!Array<!TemplateInstanceBase>}
     */
    this.instances_ = [];
  }

  /** @override */
  ready() {
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
      this.ensureDomItemAvailable_(this.items.length - 1);
    }
  }

  ensureTemplatized_() {
    // The user provided light-dom template to use when stamping DOM items.
    const template =
        /** @type {!HTMLTemplateElement} */ (this.querySelector('template'));
    assert(template, 'Template must be provided');

    this.instanceConstructor_ = templatize(template, this, {
      parentModel: true,
      instanceProps: {
        'index': true,
        'item': true,
      },
    });
  }

  /**
   * Create a DOM item and immediately insert it in the DOM tree. A reference is
   * stored in the instances_ array for future item lifecycle operations.
   * @param {number} index
   * @private
   */
  createAndInsertDomItem_(index) {
    const instance = new this.instanceConstructor_({
      index: index,
      item: this.items[index],
    });
    this.instances_[index] = instance;
    this.insertBefore(instance.root, this.children[index + 1]);
  }

  /**
   * @return {number} The average DOM item height.
   * @private
   */
  domItemAverageHeight_() {
    // It must always be true that if this logic is invoked, there should be
    // enough DOM items rendered to estimate an item average height. This is
    // ensured by the logic that observes the items array.
    const domItemCount = assert(this.instances_.length);

    const lastDomItem = this.lastElementChild;
    return (lastDomItem.offsetTop + lastDomItem.offsetHeight) / domItemCount;
  }

  /**
   * Create and insert as many DOM items as necessary to ensure the item at the
   * specified index is present.
   * @param {number} index
   * @private
   */
  ensureDomItemAvailable_(index) {
    const shouldUpdateHeight = this.instances_.length !== index + 1;
    for (let i = this.instances_.length; i < index + 1; i++) {
      this.createAndInsertDomItem_(i);
    }

    if (shouldUpdateHeight) {
      this.updateHeight_();
    }
  }

  /**
   * @param {number} index
   * @return {!Element}
   * @private
   */
  getDomItem_(index) {
    return this.instances_[index].children[0];
  }

  /**
   * @return {number} The number of items required to fill the current
   *     viewport.
   */
  viewportItemCount_() {
    return Math.ceil(this.maxHeight / this.domItemAverageHeight_());
  }

  /**
   * @param {number} height
   * @return {boolean} Whether DOM items were created or not.
   * @private
   */
  fillViewHeight_(height) {
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
    for (let i = this.instances_.length; i < desiredDomItemCount; i++) {
      this.createAndInsertDomItem_(i);
    }

    this.dispatchEvent(
        new CustomEvent('viewport-filled', {bubbles: true, composed: true}));

    if (initialDomItemCount !== desiredDomItemCount) {
      performance.mark(`infinite_list_view_updated:${
          performance.now() - startTime}:benchmark_value`);

      return true;
    }

    return false;
  }

  /**
   * Adds additional DOM items as needed to fill the view based on user scroll
   * interactions.
   * @private
   */
  onScroll_() {
    const scrollTop = this.scrollTop;
    if (scrollTop > 0 && this.instances_.length !== this.items.length) {
      if (this.fillViewHeight_(scrollTop + this.maxHeight)) {
        this.updateHeight_();
      }
    }
  }

  /**
   * Handles key events when list item elements have focus.
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_(e) {
    // Do not interfere with any parent component that manages 'shift' related
    // key events.
    if (e.shiftKey) {
      return;
    }

    const selector = /** @type {!IronSelectorElement} */ (this.$.selector);
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
   * @param {number} height
   * @private
   */
  onMaxHeightChanged_(height) {
    this.style.maxHeight = height + 'px';
  }

  /**
   * @return {boolean} Whether a list item is selected and focused.
   * @private
   */
  isItemSelectedAndFocused_() {
    const selectedItemIndex = this.$.selector.selected;
    if (selectedItemIndex !== undefined) {
      const selectedItem = this.getDomItem_(selectedItemIndex);
      const deepActiveElement = getDeepActiveElement();

      return selectedItem === deepActiveElement ||
          (selectedItem.shadowRoot &&
           selectedItem.shadowRoot.activeElement === deepActiveElement);
    }

    return false;
  }

  /**
   * Ensures that when the items property changes, only a chunk of the items
   * needed to fill the current scroll position view are added to the DOM, thus
   * improving rendering performance.
   * @param {!Array} newItems
   * @param {!Array} oldItems
   * @private
   */
  onItemsChanged_(newItems, oldItems) {
    if (this.instanceConstructor_ === null) {
      return;
    }

    if (newItems.length === 0) {
      // If the new items array is empty, there is nothing to be rendered, so we
      // remove any DOM items present.
      this.removeDomItems_(0, this.instances_.length);
      this.resetSelected_();
    } else {
      const itemSelectedAndFocused = this.isItemSelectedAndFocused_();

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

      // Ensure the selected index is valid.
      const selector = /** @type {!IronSelectorElement} */ (this.$.selector);
      if (selector.selected >= newItems.length) {
        selector.selected = newItems.length - 1;
      }

      // Restore focus to the selected item if necessary.
      if (itemSelectedAndFocused) {
        this.getDomItem_(/** @type {number} */ (selector.selected)).focus();
      }
    }

    if (newItems.length !== oldItems.length) {
      this.updateHeight_();
    }
  }

  /**
   * @param {!Array} newItems
   * @param {!Array} oldItems
   * @private
   */
  updateDomItems_(newItems, oldItems) {
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
          this.instances_[i]['item'] = newItems[i];
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
  }

  /**
   * @param {number} index
   * @param {number} count
   * @private
   */
  removeDomItems_(index, count) {
    this.instances_.splice(index, count).forEach(instance => {
      this.removeChild(instance.children[0]);
    });

    // Update the index property of the items that followed the removed items.
    for (let i = index; i < this.instances_.length; i++) {
      this.instances_[i]['index'] = i;
    }
  }

  /**
   * Sets the height of the component based on an estimated average DOM item
   * height and the total number of items.
   * @private
   */
  updateHeight_() {
    const estScrollHeight = this.items.length > 0 ?
        this.items.length * this.domItemAverageHeight_() :
        0;
    this.$.container.style.height = estScrollHeight + 'px';
  }

  /**
   * Ensure the scroll view can fully display a preceding or following list item
   * to the one selected, if existing.
   * @private
   */
  onSelectedChanged_() {
    const selector = /** @type {!IronSelectorElement} */ (this.$.selector);
    if (selector.selected === undefined) {
      return;
    }

    const selectedIndex = /** @type {number} */ (selector.selected);
    if (selectedIndex === 0 || selectedIndex === this.items.length - 1) {
      if (this.items.length > this.viewportItemCount_()) {
        this.getDomItem_(selectedIndex).scrollIntoView({behavior: 'smooth'});
      }
    } else {
      const previousItem = this.getDomItem_(selector.selected - 1);
      if (previousItem.offsetTop < this.scrollTop) {
        previousItem.scrollIntoView({behavior: 'smooth', block: 'nearest'});
        return;
      }

      const nextItemIndex = /** @type {number} */ (selector.selected) + 1;
      if (nextItemIndex < this.items.length) {
        this.ensureDomItemAvailable_(nextItemIndex);

        const nextItem = this.getDomItem_(nextItemIndex);
        if (nextItem.offsetTop + nextItem.offsetHeight >
            this.scrollTop + this.offsetHeight) {
          nextItem.scrollIntoView({behavior: 'smooth', block: 'nearest'});
        }
      }
    }
  }

  /**
   * @param {string} key Keyboard event key value.
   * @param {boolean=} focusItem Whether to focus the selected item.
   */
  navigate(key, focusItem) {
    const selector = /** @type {!IronSelectorElement} */ (this.$.selector);

    if ((key === 'ArrowUp' && selector.selected === 0) || key === 'End') {
      this.ensureDomItemAvailable_(this.items.length - 1);
      selector.selected = this.items.length - 1;
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
          this.$.selector.selected = this.items.length - 1;
          break;
      }
    }

    if (focusItem) {
      selector.selectedItem.focus({preventScroll: true});
    }
  }

  /**
   * Resets the selector's selection to the undefined state. This method
   * suppresses a closure validation that would require modifying the
   * IronSelectableBehavior's annotations for the selected property.
   * @suppress {checkTypes}
   * @private
   */
  resetSelected_() {
    /** @type {!IronSelectorElement} */ (this.$.selector).selected = undefined;
  }

  /** @param {number} index */
  set selected(index) {
    if (index === NO_SELECTION) {
      this.resetSelected_();
      return;
    }

    const selector = /** @type {!IronSelectorElement} */ (this.$.selector);
    if (index !== selector.selected) {
      assert(index < this.items.length);
      this.ensureDomItemAvailable_(index);
      selector.selected = index;
    }
  }

  /** @return {number} The selected index or -1 if none selected. */
  get selected() {
    return this.$.selector.selected !== undefined ? this.$.selector.selected :
                                                    NO_SELECTION;
  }
}

customElements.define(InfiniteList.is, InfiniteList);
