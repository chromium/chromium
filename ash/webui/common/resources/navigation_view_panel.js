// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_drawer/cr_drawer.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import './navigation_shared_vars.css.js';
import './page_toolbar.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SelectorItem} from './navigation_selector.js';
import {getTemplate} from './navigation_view_panel.html.js';

const navigationPageChanged = 'onNavigationPageChanged';

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
 *
 * To provide page components with initial data, include a "initialData" object
 * as part of the "addSelector()" function. Page components will then have an
 * implicit property, details, with the object provided.
 */
export class NavigationViewPanelElement extends PolymerElement {
  static get is() {
    return 'navigation-view-panel';
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
        observer: 'selectedItemChanged_',
        value: null,
      },

      /**
       * @type {!Array<!SelectorItem>}
       * @private
       */
      selectorItems_: {
        type: Array,
        value: () => [],
      },

      /**
       * This title only appears if |showToolBar| is True. Is otherwise a
       * no-opt if title is set and |showToolbar| is False.
       */
      title: {
        type: String,
        value: '',
      },

      /**
       * If |hasSearch| is True, the toolbar internal widths will be adjusted
       * to fit the search bar when |showNav| is False.
       */
      hasSearch: {
        type: Boolean,
      },

      /**
       * Can only be set to True if specified from the parent element by
       * adding show-banner as an attribute to <navigation-view-panel>. If
       * True, a banner will appear above the 2 column view (sidebar +
       * page). If False, banner grid-area will not show and regular grid
       * layout will be used based on show-tool-bar property.
       * @type {boolean}
       */
      showBanner: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * Can only be set to True if specified from the parent element by
       * adding show-tool-bar as an attribute to <navigation-view-panel>. If
       * True, a toolbar will appear at the top of the navigation view panel
       * with a 2 column view below it (sidebar + page). If False,
       * navigation view panel will only be a 2 column view (sidebar +
       * page).
       */
      showToolBar: {
        type: Boolean,
        value: false,
      },

      /** @protected {boolean} */
      showNav: {
        type: Boolean,
      },
    };
  }

  /** @override */
  constructor() {
    super();
    window.addEventListener('menu-tap', () => this.onMenuButtonTap_());
    window.addEventListener(
        'navigation-selected', () => this.onNavigationSelected_());

    /**
     * Event callback for 'scroll'.
     * @private {?Function}
     */
    this.scrollClassHandler_ = () => {
      this.onScroll_();
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    window.addEventListener('scroll', this.scrollClassHandler_);
  }

  /**
   * @param {string} name
   * @param {string} pageIs
   * @param {string} icon
   * @param {?string} id
   * @param {?Object} initialData
   * @return {!SelectorItem}
   */
  createSelectorItem(
      name, pageIs, icon = '', id = null, initialData = null) {
    id = id || pageIs;
    return {name, pageIs, icon, id, initialData};
  }

  /**
   * Set the initially active page (defaults to the first selector item),
   * Callers can override this default behavior by providing a
   * query param including the id of a specific page.
   * @protected
   */
  setDefaultPage_() {
    assert(!this.selectedItem);
    const params = new URLSearchParams(window.location.search);

    for (const item of this.selectorItems_) {
      if (params.has(item.id)) {
        this.selectedItem = item;
        return;
      }
    }

    // Default to first entry if query param isn't provided.
    this.selectedItem = this.selectorItems_[0];
  }

  /**
   * @param {!Array<!SelectorItem>} pages
   */
  addSelectors(pages) {
    this.set('selectorItems_', pages);
    this.setDefaultPage_();
  }

  /**
   * Adds a new section to the top level navigation. The name and icon will
   * be displayed in the side navigation. The content panel will create an
   * instance of pageIs when navigated to. If id is null it will default to
   * being equal to pageIs. In the case of adding multiple pages of the same
   * type, id must be specified to distinguish them.
   * @param {string} name
   * @param {string} pageIs
   * @param {string} icon
   * @param {?string} id
   * @param {?Object} initialData
   */
  addSelector(name, pageIs, icon = '', id = null, initialData = null) {
    this.addSelectorItem(
        this.createSelectorItem(name, pageIs, icon, id, initialData));
  }

  /**
   * Adds a new section to the top level navigation. The name and icon will
   * be displayed in the side navigation.
   * @param {!SelectorItem} selectorItem
   */
  addSelectorItem(selectorItem) {
    this.push('selectorItems_', selectorItem);
  }

  /**
   * Removes a section from the top level navigation. If the section is
   * currently selected, the selection will be reset to the top item.
   *
   * @param {string} id The ID of the section to remove.
   */
  removeSelectorById(id) {
    const index =
        this.selectorItems_.findIndex((selector) => selector.id === id);
    if (index < 0) {
      throw new Error('Cannot find selector with ID "' + id + '" to remove.');
    }
    if (this.selectorItems_.length === 1) {
      throw new Error('Removing the last selector is not supported.');
    }
    this.splice('selectorItems_', index, 1);
    if (this.selectedItem && this.selectedItem.id === id) {
      this.selectedItem = this.selectorItems_[0];
    }
  }

  /** @protected */
  selectedItemChanged_() {
    if (!this.selectedItem) {
      return;
    }
    const pageComponent = this.getPage_(this.selectedItem);

    if (this.$.drawer.open) {
      this.$.drawer.close();
    }

    this.showPage_(pageComponent);

    this.notifyEvent(navigationPageChanged);
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
      if (typeof functionCall === 'function') {
        if (functionName === navigationPageChanged) {
          const event = {isActive: this.selectedItem.id === c.id};
          functionCall.call(c, event);
        } else {
          functionCall.call(c, params);
        }
      }
    });
  }

  /**
   * Selects the page that has the given id.
   * @param {string} id
   */
  selectPageById(id) {
    const selectorItem = this.selectorItems_.find(item => item.id == id);
    if (selectorItem) {
      this.selectedItem = selectorItem;
    }
  }

  /**
   * @param {!SelectorItem} item
   * @private
   */
  getPage_(item) {
    let pageComponent = this.shadowRoot.querySelector(`#${item.id}`);

    if (pageComponent === null) {
      pageComponent = document.createElement(item.pageIs);
      assert(pageComponent);
      pageComponent.setAttribute('id', item.id);
      pageComponent.setAttribute('class', 'view-content');

      if (item.initialData) {
        pageComponent.initialData = item.initialData;
      }

      pageComponent.hidden = true;

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

  onMenuButtonTap_() {
    this.$.drawer.toggle();
  }

  /** @private */
  onScroll_() {
    if (this.showToolBar) {
      const scrollTop = document.documentElement.scrollTop;
      if (scrollTop <= 0) {
        this.shadowRoot.querySelector('page-toolbar').removeAttribute('shadow');
        return;
      }
      this.shadowRoot.querySelector('page-toolbar').setAttribute('shadow', '');
    }
  }

  /**
   * @param {string} selectorId The ID of the section to search for.
   * @return {boolean}
   */
  pageExists(selectorId) {
    return !!this.selectorItems_.find(({id}) => id === selectorId);
  }

  /** @private */
  onNavigationSelected_() {
    // Don't toggle, but rather only close the drawer if it's opened.
    if (this.$.drawer.open) {
      this.$.drawer.close();
    }
  }
}

customElements.define(NavigationViewPanelElement.is,
    NavigationViewPanelElement);
