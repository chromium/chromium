// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';

/**
 * @typedef {{
 *   name: string,
 *   pageIs: string,
 * }}
 */
export let SelectorItem;

/**
 * @typedef {{
 *   isCollapsible: boolean,
 *   isExpanded: boolean,
 *   subMenuItems: ?Array<?SelectorItem>
 * }}
 */
export let SelectorProperties;

/**
 * @typedef {{
 *   selectorItem: !SelectorItem,
 *   properties: !SelectorProperties,
 * }}
 */
export let MenuSelectorItem;

/**
 * @fileoverview
 * 'navigation-selector' is a side bar navigation element. To populate the
 * navigation-selector you must provide the element with an array of
 * MenuSelectorItem's.
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
        notify: true,
      },

      /**
       * @type {!Array<!MenuSelectorItem>}
       */
      menuItems: {
        type: Array,
        value: () => [],
      }
    }
  }

  /**
   * @param {Event} e
   * @private
   */
  onSelected_(e) {
    this.selectedItem = e.model.item.selectorItem;
  }

  /**
   * @param {Event} e
   * @private
   */
  onNestedSelected_(e) {
    this.selectedItem = e.model.item;
  }

  /**
   * @param {Event} e
   * @private
   */
  onExpandClicked_(e) {
    const selectedMenuItem = e.model.item;
    const foundIndex = this.menuItems.findIndex((element) => {
      return element.selectorItem.name === selectedMenuItem.selectorItem.name;
    });

    this.menuItems[foundIndex].properties.isExpanded =
        !this.menuItems[foundIndex].properties.isExpanded;
    this.notifyPath(`menuItems.${foundIndex}.properties.isExpanded`);
  }

  /**
   * @param {!MenuSelectorItem} item
   * @private
   */
  isCollapsible_(item) {
    return item.properties.isCollapsible;
  }
}

customElements.define(NavigationSelectorElement.is, NavigationSelectorElement);