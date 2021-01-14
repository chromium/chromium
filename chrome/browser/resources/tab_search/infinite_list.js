// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'infinite-list' is a component optimized for showing a
 * list of items that overflows the view and requires scrolling. For performance
 * reasons, The DOM items are added incrementally to the view as the user
 * scrolls through the list. The template inside this element represents the DOM
 * to create for each list item. The `items` property specifies an array of list
 * item data. The component leverages an <iron-selector> to manage item
 * selection and styling and a <dom-repeat> which renders the provided template.
 *
 * Note that the component expects a '--list-max-height' variable to be defined
 * in order to determine its maximum height. Additionally, it expects the
 * `chunkItemCount` property to be a number of DOM items that is large enough to
 * fill the view.
 */

import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';

import {assert, assertInstanceof} from 'chrome://resources/js/assert.m.js';
import {updateListProperty} from 'chrome://resources/js/list_property_update_behavior.m.js';
import {listenOnce} from 'chrome://resources/js/util.m.js';
import {afterNextRender, DomRepeat, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @type {number} */
export const NO_SELECTION = -1;

/** @type {!Array<string>} */
export const selectorNavigationKeys = ['ArrowUp', 'ArrowDown', 'Home', 'End'];

export class InfiniteList extends PolymerElement {
  static get is() {
    return 'infinite-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Controls the number of list items rendered initially, and added on
       * demand as the component is scrolled.
       */
      chunkItemCount: {
        type: Number,
        value: 10,
      },

      /** @type {?Array<!Object>} */
      items: {
        type: Array,
        observer: 'onItemsChanged_',
      },
    };
  }

  constructor() {
    super();

    /**
     * An instance of DomRepeat in charge of stamping DOM item elements.
     * For performance reasons, the items property of this instance is
     * modified on scroll events so that it has enough items to render
     * the current scroll view.
     * @private {?DomRepeat}
     */
    this.domRepeat_ = null;
  }

  /** @override */
  ready() {
    super.ready();

    this.domRepeat_ = assertInstanceof(
        this.firstChild, DomRepeat,
        'infinite-list requires a dom-repeat child to be provided in light-dom');

    this.addEventListener('scroll', () => this.onScroll_());
  }

  /** @private */
  getDomItems_() {
    const selector = /** @type {!IronSelectorElement} */ (this.$.selector);
    return Array.prototype.slice.call(
        selector.children, 0, selector.children.length - 1);
  }

  /**
   * @param {number} idx
   * @private
   */
  isDomItemAtIndexAvailable_(idx) {
    return idx < this.domRepeat_.items.length;
  }

  ensureAllDomItemsAvailable() {
    const lastItemIndex = this.items.length - 1;
    if (!this.isDomItemAtIndexAvailable_(lastItemIndex)) {
      this.ensureDomItemsAvailableStartingAt_(lastItemIndex);
    }
  }

  /**
   * Ensure we have the required DOM items to fill the current view starting
   * at the specified index.
   *
   * @param {number} idx
   * @private
   */
  ensureDomItemsAvailableStartingAt_(idx) {
    if (this.domRepeat_.items.length === this.items.length) {
      return;
    }

    const newItems = this.items.slice(
        this.domRepeat_.items.length,
        Math.min(idx + this.chunkItemCount, this.items.length));
    if (newItems.length > 0) {
      const startTime = performance.now();
      this.domRepeat_.push('items', ...newItems);
      listenOnce(this, 'dom-change', () => {
        afterNextRender(this, () => {
          performance.mark(`infinite_list_updated:${
              performance.now() - startTime}:benchmark_value`);
        });
      });
    }
  }

  /**
   * Adds additional DOM items as needed to fill the view based on user scroll
   * interactions.
   * @private
   */
  onScroll_() {
    if (this.scrollTop > 0 &&
        this.domRepeat_.items.length !== this.items.length) {
      const aboveScrollTopItemCount =
          Math.round(this.scrollTop / this.domItemAverageHeight_());

      // Ensure we have sufficient items to fill the current scroll position and
      // a full view following our current position.
      if (aboveScrollTopItemCount + this.chunkItemCount >
          this.domRepeat_.items.length) {
        this.ensureDomItemsAvailableStartingAt_(aboveScrollTopItemCount);
        this.updateScrollerSize_();
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
   * @return {number}
   * @private
   */
  domItemAverageHeight_() {
    const selector = /** @type {!IronSelectorElement} */ (this.$.selector);
    if (!selector.items || selector.items.length === 0) {
      return 0;
    }

    const domItemCount = selector.items.length;
    const lastDomItem = selector.items[domItemCount - 1];
    return (lastDomItem.offsetTop + lastDomItem.offsetHeight) / domItemCount;
  }

  /**
   * Ensures that when the items property changes, only a chunk of the items
   * needed to fill the current scroll position view are added to the DOM, thus
   * improving rendering performance.
   *
   * @param {!Array} newItems
   * @param {!Array} oldItems
   * @private
   */
  onItemsChanged_(newItems, oldItems) {
    if (!this.domRepeat_) {
      return;
    }

    if (!oldItems || oldItems.length === 0) {
      this.domRepeat_.set('items', []);
      this.ensureDomItemsAvailableStartingAt_(0);
      listenOnce(this.$.selector, 'iron-items-changed', () => {
        this.updateScrollerSize_();
      });

      return;
    }

    updateListProperty(
        this.domRepeat_, 'items', tabData => tabData,
        newItems.slice(
            0,
            Math.min(
                Math.max(this.domRepeat_.items.length, this.chunkItemCount),
                newItems.length)),
        true /* identityBasedUpdate= */);

    if (newItems.length !== oldItems.length) {
      this.updateScrollerSize_();
    }
  }

  /**
   * Sets the scroll height of the component based on an estimated average
   * DOM item height and the total number of items.
   * @private
   */
  updateScrollerSize_() {
    if (this.$.selector.items.length !== 0) {
      const estScrollHeight = this.items.length * this.domItemAverageHeight_();
      this.$.items.style.height = estScrollHeight + 'px';
    }
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

    const selectedIndex = /** @type{number} */ (selector.selected);
    if (selectedIndex === 0 || selectedIndex === this.items.length - 1) {
      /** @type {!Element} */ (selector.selectedItem).scrollIntoView({
        behavior: 'smooth'
      });
    } else {
      // If the following DOM item to the currently selected item has not yet
      // been rendered, ensure it is by waiting for the next render frame
      // before we scroll it into the view.
      if (!this.isDomItemAtIndexAvailable_(selectedIndex + 1)) {
        this.ensureDomItemsAvailableStartingAt_(selectedIndex + 1);

        afterNextRender(this, this.onSelectedChanged_);
        return;
      }

      const previousItem = selector.items[selector.selected - 1];
      if (previousItem.offsetTop < this.scrollTop) {
        /** @type {!Element} */ (previousItem)
            .scrollIntoView({behavior: 'smooth', block: 'nearest'});
        return;
      }

      const nextItem =
          selector.items[/** @type {number} */ (selector.selected) + 1];
      if (nextItem.offsetTop + nextItem.offsetHeight >
          this.scrollTop + this.offsetHeight) {
        /** @type {!Element} */ (nextItem).scrollIntoView(
            {behavior: 'smooth', block: 'nearest'});
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
      // If the DOM item to be selected has not yet been rendered, ensure it is
      // by waiting for the next render frame.
      const lastItemIndex = this.items.length - 1;
      if (!this.isDomItemAtIndexAvailable_(lastItemIndex)) {
        this.ensureDomItemsAvailableStartingAt_(lastItemIndex);

        afterNextRender(
            this, /** @type {function(...*)} */ (this.navigate),
            [key, focusItem]);
        return;
      }
    }

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

    if (focusItem) {
      selector.selectedItem.focus({preventScroll: true});
    }
  }

  /** @param {number} index */
  set selected(index) {
    const selector = /** @type {!IronSelectorElement} */ (this.$.selector);
    if (index !== selector.selected) {
      selector.selected = index;

      if (index !== NO_SELECTION) {
        assert(index < this.items.length);
        this.ensureDomItemsAvailableStartingAt_(index);
      }
    }
  }

  /** @return {number} The selected index or -1 if none selected. */
  get selected() {
    return this.$.selector.selected !== undefined ? this.$.selector.selected :
                                                    NO_SELECTION;
  }
}

customElements.define(InfiniteList.is, InfiniteList);
