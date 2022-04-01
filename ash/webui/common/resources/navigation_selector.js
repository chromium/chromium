// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import './navigation_icons.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

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
    return html`{__html_template__}`;
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
      }
    };
  }

  /**
   * @param {Event} e
   * @private
   */
  onSelected_(e) {
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
      } else {
        item.classList.remove('selected');
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