// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MenuSelectorItem, SelectorItem, SelectorProperties} from './navigation_selector.js';

/**
 * @fileoverview
 * 'navigation-view-panel' manages the wiring between a display page and
 * <navigation-selector>.
 *
 * Child pages that are interested in navigation page change events will need to
 * implement a public function "onNavigationPageChanged()" to be notified of the
 * event.
 *
 * To send events between pages, the component that has <navigation-view-panel>
 * must call on "notifyEvent(functionName, params)". |params| is an optional
 * parameter.
 */
export class NavigationViewPanelElement extends PolymerElement {
  static get is() {
    return 'navigation-view-panel';
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
        observer: "onSwitchPage_",
        value: null,
      },

      /**
       * @type {!Array<!MenuSelectorItem>}
       * @private
       */
      menuItems_: {
        type: Array,
        value: () => [],
      }
    }
  }

  /**
   * @param {string} name
   * @param {string} pageIs
   * @param {string} icon
   * @param {!Array<SelectorItem>} subItems
   */
  addSelector(name, pageIs, icon='', subItems=[]) {
    let item = /** @type {SelectorItem} */ ({
        'name': name, 'pageIs': pageIs, 'icon': icon});
    let property = /** @type {SelectorProperties} */ ({
        'isCollapsible': subItems.length,
        'isExpanded': false,
        'subMenuItems': subItems,
    });
    let menuItem = /** @type {!MenuSelectorItem} */ ({
        'selectorItem': item,
        'properties': property,
    });

    this.push('menuItems_', menuItem);
    // Set the initial default page, if the first entry is a collapsible entry
    // the initial page is the first sub menu item. Otherwise, the first entry
    // is the first menu item.
    if (!this.selectedItem) {
      if (property.isCollapsible) {
        this.selectedItem = property.subMenuItems[0];
      } else {
        this.selectedItem = item;
      }
    }
  }

  /**
   * @param {!SelectorItem} current
   * @param {?SelectorItem} previous
   * @private
   */
  onSwitchPage_(current, previous) {
    if (!this.selectedItem)
      return;
    const pageComponent = this.getPage_(this.selectedItem);
    this.showPage_(pageComponent);

    const event = {previous: !!previous ? previous.pageIs : '',
                   current: current.pageIs};
    this.notifyEvent('onNavigationPageChanged', event);
  }

  /**
   * @param {string} functionName
   * @param {!Object} params
   */
  notifyEvent(functionName, params={}) {
    const components = this.shadowRoot.querySelectorAll('.view-content');
    // Notify all available child pages of the event.
    Array.from(components).map((c) => {
      const functionCall = c[functionName];
      if (typeof functionCall === "function") {
        functionCall.call(c, {detail: params});
      }
    });
  }

  /**
   * @param {!SelectorItem} item
   * @private
   */
  getPage_(item) {
    const pageName = item.pageIs;
    let pageComponent = this.shadowRoot.querySelector(`#${item.pageIs}`);

    if (pageComponent === null) {
      pageComponent = document.createElement(pageName);
      pageComponent.setAttribute('id', pageName);
      pageComponent.setAttribute('class', 'view-content');
      pageComponent.hidden = true;
      if (!pageComponent) {
        console.error('Failed to create ' + pageName);
      }
      this.$.navigationBody.appendChild(pageComponent);
    }
    return pageComponent;
  }

  /**
   * @param {!HTMLElement} pageComponent
   * @private
   */
  showPage_(pageComponent) {
    const components = this.shadowRoot.querySelectorAll('.view-content');
    // Hide all existing pages.
    Array.from(components).map((c) => c.hidden = true);
    pageComponent.hidden = false;
  }
}

customElements.define(NavigationViewPanelElement.is,
    NavigationViewPanelElement);
