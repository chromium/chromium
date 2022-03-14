// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/mwb_element_shared_style.js';
import 'chrome://resources/cr_elements/mwb_shared_style.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import './read_later_item.js';
import './strings.m.js';

import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {listenOnce} from 'chrome://resources/js/util.m.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CurrentPageActionButtonState, ReadLaterEntriesByStatus, ReadLaterEntry} from './read_later.mojom-webui.js';
import {ReadLaterApiProxy, ReadLaterApiProxyImpl} from './read_later_api_proxy.js';
import {ReadLaterItemElement} from './read_later_item.js';

const navigationKeys: Set<string> = new Set(['ArrowDown', 'ArrowUp']);

export interface ReadLaterAppElement {
  $: {
    readLaterList: HTMLElement,
    selector: IronSelectorElement,
  },
}

export class ReadLaterAppElement extends PolymerElement {
  static get is() {
    return 'read-later-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      unreadItems_: {
        type: Array,
        value: [],
      },

      readItems_: {
        type: Array,
        value: [],
      },

      currentPageActionButtonState_: {
        type: Number,
        value: CurrentPageActionButtonState.kDisabled,
      },

      buttonRipples: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('useRipples'),
      },

      loadingContent_: {
        type: Boolean,
        value: true,
      },

      unifiedSidePanel_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('unifiedSidePanel'),
      },
    };
  }

  private unreadItems_: ReadLaterEntry[];
  private readItems_: ReadLaterEntry[];
  private currentPageActionButtonState_: CurrentPageActionButtonState;
  buttonRipples: boolean;
  private loadingContent_: boolean;
  private unifiedSidePanel_: boolean;
  private apiProxy_: ReadLaterApiProxy = ReadLaterApiProxyImpl.getInstance();
  private listenerIds_: number[] = [];
  private visibilityChangedListener_: () => void;

  constructor() {
    super();

    this.visibilityChangedListener_ = () => {
      // Refresh Read Later's list data when transitioning into a visible state.
      if (document.visibilityState === 'visible') {
        this.updateReadLaterEntries_();
      }
    };
  }

  override connectedCallback() {
    super.connectedCallback();

    document.addEventListener(
        'visibilitychange', this.visibilityChangedListener_);

    const callbackRouter = this.apiProxy_.getCallbackRouter();
    this.listenerIds_.push(
        callbackRouter.itemsChanged.addListener(
            (entries: ReadLaterEntriesByStatus) => this.updateItems_(entries)),
        callbackRouter.currentPageActionButtonStateChanged.addListener(
            (state: CurrentPageActionButtonState) =>
                this.updateCurrentPageActionButton_(state)));

    if (this.unifiedSidePanel_) {
      this.apiProxy_.showUI();
    }

    // If added in a visible state update current read later items.
    if (document.visibilityState === 'visible') {
      this.updateReadLaterEntries_();
      this.apiProxy_.updateCurrentPageActionButtonState();
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));

    document.removeEventListener(
        'visibilitychange', this.visibilityChangedListener_);
  }

  /**
   * Fetches the latest read later entries from the browser.
   */
  private async updateReadLaterEntries_() {
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

  private updateItems_(entries: ReadLaterEntriesByStatus) {
    this.unreadItems_ = entries.unreadEntries;
    this.readItems_ = entries.readEntries;
    this.loadingContent_ = false;
  }

  private updateCurrentPageActionButton_(state: CurrentPageActionButtonState) {
    this.currentPageActionButtonState_ = state;
  }

  private ariaLabel_(item: ReadLaterEntry): string {
    return `${item.title} - ${item.displayUrl} - ${
        item.displayTimeSinceUpdate}`;
  }

  private shouldShowCurrentPageActionButton_(): boolean {
    return loadTimeData.getBoolean('currentPageActionButtonEnabled');
  }

  /**
   * @return The appropriate text for the empty state subheader
   */
  private getEmptyStateSubheaderText_(): string {
    if (this.shouldShowCurrentPageActionButton_()) {
      return loadTimeData.getString('emptyStateAddFromDialogSubheader');
    }
    return loadTimeData.getString('emptyStateSubheader');
  }

  /**
   * @return Whether the current page action button should be disabled
   */
  private getCurrentPageActionButtonDisabled_(): boolean {
    return this.currentPageActionButtonState_ ===
        CurrentPageActionButtonState.kDisabled;
  }

  private isReadingListEmpty_(): boolean {
    return this.unreadItems_.length === 0 && this.readItems_.length === 0;
  }

  private onCurrentPageActionButtonClick_() {
    this.apiProxy_.addCurrentTab();
  }

  private onItemKeyDown_(e: KeyboardEvent) {
    if (e.shiftKey || !navigationKeys.has(e.key)) {
      return;
    }
    switch (e.key) {
      case 'ArrowDown':
        this.$.selector.selectNext();
        (this.$.selector.selectedItem as ReadLaterItemElement).focus();
        break;
      case 'ArrowUp':
        this.$.selector.selectPrevious();
        (this.$.selector.selectedItem as ReadLaterItemElement).focus();
        break;
      default:
        assertNotReached();
    }
    e.preventDefault();
    e.stopPropagation();
  }

  private onItemFocus_(e: Event) {
    this.$.selector.selected =
        (e.currentTarget as ReadLaterItemElement).dataset.url!;
  }

  private onCloseClick_(e: Event) {
    e.stopPropagation();
    this.apiProxy_.closeUI();
  }

  private shouldShowHr_(): boolean {
    return this.unreadItems_.length > 0 && this.readItems_.length > 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'read-later-app': ReadLaterAppElement;
  }
}

customElements.define(ReadLaterAppElement.is, ReadLaterAppElement);
