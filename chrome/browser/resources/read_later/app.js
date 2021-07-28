// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/mwb_shared_style.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import './read_later_shared_style.js';
import './strings.m.js';

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {listenOnce} from 'chrome://resources/js/util.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ReadLaterApiProxy, ReadLaterApiProxyImpl} from './read_later_api_proxy.js';
import {ReadLaterItemElement} from './read_later_item.js';

/** @type {!Set<string>} */
const navigationKeys = new Set(['ArrowDown', 'ArrowUp']);

export class ReadLaterAppElement extends PolymerElement {
  static get is() {
    return 'read-later-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {!Array<!readLater.mojom.ReadLaterEntry>} */
      unreadItems_: {
        type: Array,
        value: [],
      },

      /** @private {!Array<!readLater.mojom.ReadLaterEntry>} */
      readItems_: {
        type: Array,
        value: [],
      },

      /** @private {!readLater.mojom.CurrentPageActionButtonState} */
      currentPageActionButtonState_: {
        type: Number,
        value: readLater.mojom.CurrentPageActionButtonState.kAdd,
      },

      /** @private {!url.mojom.Url} */
      currentUrl_: {
        type: Object,
        value: null,
      },

      /** @type {boolean} */
      buttonRipples: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('useRipples'),
      },
    };
  }

  constructor() {
    super();
    /** @private {!ReadLaterApiProxy} */
    this.apiProxy_ = ReadLaterApiProxyImpl.getInstance();

    /** @private {!Array<number>} */
    this.listenerIds_ = [];

    /** @private {!Function} */
    this.visibilityChangedListener_ = () => {
      // Refresh Read Later's list data when transitioning into a visible state.
      if (document.visibilityState === 'visible') {
        this.updateReadLaterEntries_();
      }
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    document.addEventListener(
        'visibilitychange', this.visibilityChangedListener_);

    const callbackRouter = this.apiProxy_.getCallbackRouter();
    this.listenerIds_.push(
        callbackRouter.itemsChanged.addListener(
            entries => this.updateItems_(entries)),
        callbackRouter.currentPageActionButtonStateChanged.addListener(
            (state, url) => this.updateCurrentPageActionButton_(state, url)));

    // If added in a visible state update current read later items.
    if (document.visibilityState === 'visible') {
      this.updateReadLaterEntries_();
    }
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));

    document.removeEventListener(
        'visibilitychange', this.visibilityChangedListener_);
  }

  /**
   * Fetches the latest read later entries from the browser.
   * @private
   */
  async updateReadLaterEntries_() {
    const getEntriesStartTimestamp = Date.now();

    const {entries} = await this.apiProxy_.getReadLaterEntries();

    chrome.metricsPrivate.recordTime(
        'ReadingList.WebUI.ReadingListDataReceived',
        Math.round(Date.now() - getEntriesStartTimestamp));

    if (entries.unreadEntries.length !== 0 ||
        entries.readEntries.length !== 0) {
      listenOnce(this.$.readLaterList, 'dom-change', () => {
        // Push ShowUI() callback to the event queue to allow deferred rendering
        // to take place.
        setTimeout(() => this.apiProxy_.showUI(), 0);
      });
    } else {
      setTimeout(() => this.apiProxy_.showUI(), 0);
    }

    this.updateItems_(entries);
  }

  /**
   * @param {!readLater.mojom.ReadLaterEntriesByStatus} entries
   * @private
   */
  updateItems_(entries) {
    this.unreadItems_ = entries.unreadEntries;
    this.readItems_ = entries.readEntries;
  }

  /**
   * @param {!readLater.mojom.CurrentPageActionButtonState} state
   * @param {!url.mojom.Url} url
   * @private
   */
  updateCurrentPageActionButton_(state, url) {
    this.currentPageActionButtonState_ = state;
    this.currentUrl_ = url;
  }

  /**
   * @param {!readLater.mojom.ReadLaterEntry} item
   * @return {string}
   * @private
   */
  ariaLabel_(item) {
    return `${item.title} - ${item.displayUrl} - ${
        item.displayTimeSinceUpdate}`;
  }

  /**
   * @return {boolean} Whether the current page action button should be disabled
   * @private
   */
  getCurrentPageActionButtonDisabled_() {
    return this.currentPageActionButtonState_ ===
        readLater.mojom.CurrentPageActionButtonState.kDisabled;
  }

  /**
   * @return {string} The appropriate class for the current state
   * @private
   */
  getCurrentPageActionButtonClass_() {
    if (this.currentPageActionButtonState_ ===
        readLater.mojom.CurrentPageActionButtonState.kRemove) {
      return 'remove';
    } else if (
        this.currentPageActionButtonState_ ===
        readLater.mojom.CurrentPageActionButtonState.kAdd) {
      return 'add';
    }
    return 'disabled';
  }

  /**
   * @return {string} The appropriate text for the current state
   * @private
   */
  getCurrentPageActionButtonText_() {
    if (this.currentPageActionButtonState_ ===
        readLater.mojom.CurrentPageActionButtonState.kAdd) {
      return loadTimeData.getString('addCurrentTab');
    } else if (
        this.currentPageActionButtonState_ ===
        readLater.mojom.CurrentPageActionButtonState.kRemove) {
      return loadTimeData.getString('removeCurrentTab');
    }
    return loadTimeData.getString('cantAddCurrentTab');
  }

  /**
   * @param {string} addIcon
   * @param {string} removeIcon
   * @return {string} The appropriate icon for the current state
   * @private
   */
  getCurrentPageActionButtonIcon_(addIcon, removeIcon) {
    if (this.currentPageActionButtonState_ ===
        readLater.mojom.CurrentPageActionButtonState.kRemove) {
      return removeIcon;
    }
    return addIcon;
  }

  /**
   * @return {boolean}
   * @private
   */
  isReadingListEmpty_() {
    return this.unreadItems_.length === 0 && this.readItems_.length === 0;
  }

  /** @private */
  onCurrentPageActionButtonClick_() {
    if (this.currentPageActionButtonState_ ===
        readLater.mojom.CurrentPageActionButtonState.kAdd) {
      this.apiProxy_.addCurrentTab();
    } else if (
        this.currentPageActionButtonState_ ===
            readLater.mojom.CurrentPageActionButtonState.kRemove &&
        !!this.currentUrl_) {
      this.apiProxy_.removeEntry(this.currentUrl_);
    }
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemKeyDown_(e) {
    if (e.shiftKey || !navigationKeys.has(e.key)) {
      return;
    }
    const selector = /** @type {!IronSelectorElement} */ (this.$.selector);
    switch (e.key) {
      case 'ArrowDown':
        selector.selectNext();
        /** @type {!ReadLaterItemElement} */ (selector.selectedItem).focus();
        break;
      case 'ArrowUp':
        selector.selectPrevious();
        /** @type {!ReadLaterItemElement} */ (selector.selectedItem).focus();
        break;
      default:
        assertNotReached();
        return;
    }
    e.preventDefault();
    e.stopPropagation();
  }

  /**
   * @param {!Event} e
   * @private
   */
  onItemFocus_(e) {
    this.$.selector.selected =
        /** @type {!ReadLaterItemElement} */ (e.currentTarget).dataset.url;
  }

  /**
   * @param {!Event} e
   * @private
   */
  onCloseClick_(e) {
    e.stopPropagation();
    this.apiProxy_.closeUI();
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowCurrentPageActionButton_() {
    return loadTimeData.getBoolean('currentPageActionButtonEnabled');
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowHr_() {
    return this.unreadItems_.length > 0 && this.readItems_.length > 0;
  }
}

customElements.define(ReadLaterAppElement.is, ReadLaterAppElement);
