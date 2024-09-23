// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './navigation_icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './navigation_selector.html.js';

/**
 * @typedef {{
 *   name: string,
 *   pageIs: string,
 *   icon: string,
 *   initialData: ?Object,
 *   id: string,
 * }}
 */
export let SelectorItem;

/**
 * @fileoverview
 * 'navigation-selector' is a side bar navigation element. To populate the
 * navigation-selector you must provide the element with an array of
 * SelectorItem's.
 */
export class NavigationSelectorElement extends PolymerElement {
  static get is() {
    return 'navigation-selector';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * @type {?SelectorItem}
       * Notifies parent elements of the the selected item.
       */
      selectedItem: {
        type: Object,
        value: null,
        observer: 'selectedItemChanged_',
        notify: true,
      },

      /**
       * @type {!Array<!SelectorItem>}
       */
      selectorItems: {
        type: Array,
        value: () => [],
      },
    };
  }

  /**
   * @param {Event} e
   * @private
   */
  onSelected_(e) {
    this.dispatchEvent(new CustomEvent(
        'navigation-selected', {bubbles: true, composed: true}));
    this.selectedItem = e.model.item;
  }

  /** @protected */
  selectedItemChanged_() {
    // Update any top-level entries.
    const items = /** @type {!NodeList<!HTMLDivElement>} */(
        this.shadowRoot.querySelectorAll('.navigation-item'));
    this.updateSelected_(items);
  }

  /**
   * @param {!NodeList<!HTMLDivElement>} items
   * @private
   */
  updateSelected_(items) {
    for (const item of items) {
      if (item.textContent.trim() === this.selectedItem.name) {
        item.classList.add('selected');
        item.setAttribute('aria-current', 'true');
      } else {
        item.classList.remove('selected');
        item.removeAttribute('aria-current');
      }
    }
  }

  /**
   * @param {!SelectorItem} item
   * @return {string}
   * @protected
   */
  getIcon_(item) {
    return item.icon;
  }

  /**
   * @param {!SelectorItem} item
   * @return {string}
   * @protected
   */
  computeInitialClass_(item) {
    let classList = 'navigation-item';
    if (!!this.selectedItem && item.name == this.selectedItem.name) {
      // Add the initial .selected class to the currently selected entry.
      classList += ' selected';
    }
    return classList;
  }
}

customElements.define(NavigationSelectorElement.is, NavigationSelectorElement);
