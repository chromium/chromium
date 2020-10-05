// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/mwb_shared_style.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import './tab_search_item.js';
import './tab_search_search_field.js'
import './strings.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {isMac} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {listenOnce} from 'chrome://resources/js/util.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {fuzzySearch} from './fuzzy_search.js';
import {TabData} from './tab_data.js';
import {TabSearchApiProxy, TabSearchApiProxyImpl} from './tab_search_api_proxy.js';

const selectorNavigationKeys = ['ArrowUp', 'ArrowDown', 'Home', 'End'];

export class TabSearchAppElement extends PolymerElement {
  static get is() {
    return 'tab-search-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {string} */
      searchText_: {
        type: String,
        value: '',
      },

      /** @private {?Array<!tabSearch.mojom.WindowTabs>} */
      openTabs_: {
        type: Array,
        observer: 'openTabsChanged_',
      },

      /** @private {!Array<!TabData>} */
      filteredOpenTabs_: {
        type: Array,
        value: [],
      },

      /**
       * Controls the number of tab search list items initially rendered in
       * dom-repeat's chunked rendering mode.
       * @private {number}
       */
      chunkingItemCount_: {
        type: Number,
        value: 10,
      },

      /**
       * Options for fuzzy search.
       * @private {!Object}
       */
      fuzzySearchOptions_: {
        type: Object,
        value: {
          includeScore: true,
          includeMatches: true,
          ignoreLocation: true,
          threshold: 0.0,
          distance: 200,
          keys: [
            {
              name: 'tab.title',
              weight: 2,
            },
            {
              name: 'hostname',
              weight: 1,
            }
          ],
        },
      },

      /** @private {boolean} */
      feedbackButtonEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('submitFeedbackEnabled'),
      },
    };
  }

  constructor() {
    super();

    /** @private {!TabSearchApiProxy} */
    this.apiProxy_ = TabSearchApiProxyImpl.getInstance();

    /** @private {!Array<number>} */
    this.listenerIds_ = [];
  }

  /** @override */
  ready() {
    super.ready();
    this.addEventListener(
        'keydown',
        (e) => {this.onKeyDown_(/** @type {!KeyboardEvent} **/ (e))});

    // Update option values for fuzzy search from feature params.
    this.fuzzySearchOptions_ = Object.assign({}, this.fuzzySearchOptions_, {
      ignoreLocation: loadTimeData.getBoolean('searchIgnoreLocation'),
      threshold: loadTimeData.getValue('searchThreshold'),
      distance: loadTimeData.getInteger('searchDistance'),
      keys: [
        {
          name: 'tab.title',
          weight: loadTimeData.getValue('searchTitleToHostnameWeightRatio'),
        },
        {
          name: 'hostname',
          weight: 1,
        }
      ],
    });

    // TODO(tluk): The listener should provide the data needed to update the
    // WebUI without having to make another round trip request to the Browser.
    const callbackRouter = this.apiProxy_.getCallbackRouter();
    this.listenerIds_.push(
        callbackRouter.tabsChanged.addListener(() => this.updateTabs_()),
        callbackRouter.tabUpdated.addListener(tab => this.onTabUpdated_(tab)));
    this.updateTabs_();
  }

  /** @override */
  disconnectedCallback() {
    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));
  }

  /** @private */
  updateTabs_() {
    const getTabsStartTimestamp = Date.now();
    this.apiProxy_.getProfileTabs().then(({profileTabs}) => {
      chrome.metricsPrivate.recordTime(
          'Tabs.TabSearch.WebUI.TabListDataReceived',
          Math.round(Date.now() - getTabsStartTimestamp));

      // Prior to the first load |this.openTabs_| has not been set. Record the
      // time it takes for the initial list of tabs to render.
      if (!this.openTabs_) {
        listenOnce(this.$.tabsList, 'rendered-item-count-changed', e => {
          const event = /** @type {!CustomEvent<!{value: number}>} */ (e);
          // The initial rendered tab list must be non-zero.
          assert(event.detail.value > 0);

          // Chunking is used to bound the time to interactive for users
          // irrespective of the number of tabs they have open. This is no longer
          // needed after the initial list render and can cause flickering on
          // updates so disable it here.
          // TODO(tluk): Investigate a more efficient way to handle this.
          this.chunkingItemCount_ = 0;

          // Push showUI() to the event loop to allow reflow to occur following
          // the DOM update.
          setTimeout(() => {
            this.apiProxy_.showUI();
            chrome.metricsPrivate.recordTime(
              'Tabs.TabSearch.WebUI.InitialTabsRenderTime',
              Math.round(window.performance.now()));
          }, 0);
        });
      }
      this.openTabs_ = profileTabs.windows;
    });
  }

  /**
   * @param {!tabSearch.mojom.Tab} updatedTab
   * @private
   */
  onTabUpdated_(updatedTab) {
    const updatedTabId = updatedTab.tabId;
    const windows = this.openTabs_;
    if (windows) {
      for (const window of windows) {
        const {tabs} = window;
        for (let i = 0; i < tabs.length; ++i) {
          // Replace the tab with the same tabId and trigger rerender.
          if (tabs[i].tabId === updatedTabId) {
            tabs[i] = updatedTab;
            this.openTabs_ = windows.concat();
            return;
          }
        }
      }
    }
  }

  /**
   * The seleted item's index, or -1 if no item selected.
   * @return {number}
   */
  getSelectedIndex() {
    const selector = /** @type {!IronSelectorElement} */ (this.$.selector);
    return selector.selected !== undefined ?
        /** @type {number} */ (selector.selected) :
        -1;
  }

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  onSearchChanged_(e) {
    this.searchText_ = e.detail;

    this.updateFilteredTabs_(this.openTabs_ || []);
    // Reset the selected item whenever a search query is provided.
    this.$.selector.selected =
        this.filteredOpenTabs_.length > 0 ? 0 : undefined;
    this.$.tabs.scrollTop = 0;

    const length = this.filteredOpenTabs_.length;
    let text;
    if (this.searchText_.length > 0) {
      text = loadTimeData.getStringF(
          length == 1 ? 'a11yFoundTabFor' : 'a11yFoundTabsFor', length,
          this.searchText_);
    } else {
      text = loadTimeData.getStringF(
          length == 1 ? 'a11yFoundTab' : 'a11yFoundTabs', length);
    }
    this.announceA11y_(text);
  }

  /** @private */
  onFeedbackClick_() {
    this.apiProxy_.showFeedbackPage();
  }

  /** @private */
  onFeedbackFocus_() {
    this.$.selector.selected = undefined;
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemClick_(e) {
    const tabId = Number.parseInt(e.currentTarget.id, 10);
    this.apiProxy_.switchToTab({tabId}, !!this.searchText_);
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemClose_(e) {
    const tabId = Number.parseInt(e.currentTarget.id, 10);
    this.apiProxy_.closeTab(tabId);
    this.announceA11y_(loadTimeData.getString('a11yTabClosed'));
  }

  /**
   * @param {!Array<!tabSearch.mojom.WindowTabs>} newOpenTabs
   * @private
   */
  openTabsChanged_(newOpenTabs) {
    this.updateFilteredTabs_(newOpenTabs);

    // If there was no previously selected index, set the first item as
    // selected; else retain the currently selected index. If the list
    // shrunk above the selected index, select the last index in the list.
    // If there are no matching results, set the selected index value to none.
    this.$.selector.selectIndex(Math.min(
        Math.max(this.getSelectedIndex(), 0),
        this.filteredOpenTabs_.length - 1));
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemFocus_(e) {
    this.$.selector.selected =
        e.currentTarget.parentNode.indexOf(e.currentTarget);
    this.updateScrollView_();
  }

  /**
   * @param {string} key Keyboard event key value.
   * @private
   */
  selectorNavigate_(key) {
    const selector = /** @type {!IronSelectorElement} */ (this.$.selector);
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
        selector.selected = this.filteredOpenTabs_.length - 1;
        break;
    }
  }

  /**
   * Handles key events when list item elements have focus.
   * @param {!KeyboardEvent} e
   * @private
   */
  onItemKeyDown_(e) {
    if (e.shiftKey) {
      return;
    }

    if (this.getSelectedIndex() === -1) {
      // No tabs matching the search text criteria.
      return;
    }

    if (selectorNavigationKeys.includes(e.key)) {
      this.selectorNavigate_(e.key);
      /** @type {!HTMLElement} */ (this.$.selector.selectedItem).focus({
        preventScroll: true
      });
      e.stopPropagation();
      e.preventDefault();
    } else if (e.key === 'Enter' || e.key === ' ') {
      this.apiProxy_.switchToTab(
          {tabId: this.getSelectedTab_().tabId}, !!this.searchText_);
      e.stopPropagation();
    }
  }

  /** @private */
  onSearchFocus_() {
    if (this.$.selector.selected === undefined &&
        this.filteredOpenTabs_.length > 0) {
      this.$.selector.selectIndex(0);
    }
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_(e) {
    if (e.key === 'Escape') {
      e.stopPropagation();
      e.preventDefault();
      this.apiProxy_.closeUI();
    }
  }

  /**
   * Handles key events when the search field has focus.
   * @param {!KeyboardEvent} e
   * @private
   */
  onSearchKeyDown_(e) {
    // Do not interfere with the search field's management of text selection.
    if (e.shiftKey) {
      return;
    }

    if (this.getSelectedIndex() === -1) {
      // No tabs matching the search text criteria.
      return;
    }

    if (selectorNavigationKeys.includes(e.key)) {
      this.selectorNavigate_(e.key);
      this.updateScrollView_();
      e.stopPropagation();
      e.preventDefault();

      // For some reasons setting combobox/aria-activedescendant on tab-search-search-field
      // has no effect, so manually announce a11y message here.
      this.announceA11y_(this.ariaLabel_(this.getSelectedTab_()));
    } else if (e.key === 'Enter') {
      this.apiProxy_.switchToTab(
          {tabId: this.getSelectedTab_().tabId}, !!this.searchText_);
      e.stopPropagation();
    }
  }

  /** @param {string} text */
  announceA11y_(text) {
    IronA11yAnnouncer.requestAvailability();
    this.dispatchEvent(new CustomEvent(
        'iron-announce', {bubbles: true, composed: true, detail: {text}}));
  }

  /**
   * @return {string}
   * @private
   */
  ariaLabel_(item) {
    return `${item.title} ${item.hostname}`;
  }

  /**
   * @return {string}
   * @private
   */
  getKeyboardShortcut_() {
    return (isMac ? 'Cmd' : 'Ctrl') + '+Shift+A';
  }

  /**
   * @param {!Array<!tabSearch.mojom.WindowTabs>} windowTabs
   * @private
   */
  updateFilteredTabs_(windowTabs) {
    const result = [];
    windowTabs.forEach(window => {
      window.tabs.forEach(tab => {
        const hostname = new URL(tab.url).hostname;
        result.push({hostname, tab});
      });
    });
    result.sort(
        (a, b) => (b.tab.lastActiveTimeTicks && a.tab.lastActiveTimeTicks) ?
            b.tab.lastActiveTimeTicks.internalValue -
                a.tab.lastActiveTimeTicks.internalValue :
            0);
    this.filteredOpenTabs_ =
        fuzzySearch(this.searchText_, result, this.fuzzySearchOptions_);

    // Update the item count in css so that the css rule can calculate the final
    // height of the tabsContainer. This prevents the scrolling height from
    // changing as list items are added to the dom incrementally via chunking
    // mode.
    this.$.tabsContainer.style
      .setProperty("--item-count", this.filteredOpenTabs_.length.toString());
  }

  /**
   * Ensure the scroll view can fully display a preceding or following tab item
   * if existing.
   * @private
   */
  updateScrollView_() {
    const selectedIndex = this.getSelectedIndex();
    if (selectedIndex === 0 ||
        selectedIndex === this.filteredOpenTabs_.length - 1) {
      /** @type {!Element} */ (this.$.selector.selectedItem).scrollIntoView({
        behavior: 'smooth'
      });
    } else {
      const previousItem = this.$.selector.items[this.$.selector.selected - 1];
      if (previousItem.offsetTop < this.$.tabs.scrollTop) {
        /** @type {!Element} */ (previousItem)
            .scrollIntoView({behavior: 'smooth', block: 'nearest'});
        return;
      }

      const nextItem = this.$.selector.items[this.$.selector.selected + 1];
      if (nextItem.offsetTop + nextItem.offsetHeight >
          this.$.tabs.scrollTop + this.$.tabs.offsetHeight) {
        /** @type {!Element} */ (nextItem).scrollIntoView(
            {behavior: 'smooth', block: 'nearest'});
      }
    }
  }

  /** return {!tabSearch.mojom.Tab} */
  getSelectedTab_() {
    return this.filteredOpenTabs_[this.getSelectedIndex()].tab;
  }
}

customElements.define(TabSearchAppElement.is, TabSearchAppElement);
