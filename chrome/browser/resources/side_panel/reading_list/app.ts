// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/mwb_element_shared_style.css.js';
import 'chrome://resources/cr_elements/mwb_shared_style.css.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import './reading_list_item.js';
import '../strings.m.js';

import {HelpBubbleMixin, HelpBubbleMixinInterface} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {listenOnce} from 'chrome://resources/js/util_ts.js';
import {IronSelectorElement} from 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import {DomRepeat, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {CurrentPageActionButtonState, ReadLaterEntriesByStatus, ReadLaterEntry} from './reading_list.mojom-webui.js';
import {ReadingListApiProxy, ReadingListApiProxyImpl} from './reading_list_api_proxy.js';
import {MARKED_AS_READ_UI_EVENT, ReadingListItemElement} from './reading_list_item.js';

const navigationKeys: Set<string> = new Set(['ArrowDown', 'ArrowUp']);

const ReadingListAppElementBase = HelpBubbleMixin(PolymerElement) as
    {new (): PolymerElement & HelpBubbleMixinInterface};

export interface ReadingListAppElement {
  $: {
    readingListList: HTMLElement,
    selector: IronSelectorElement,
    unreadItemsList: DomRepeat,
  };
}

// browser_element_identifiers constants
const ADD_CURRENT_TAB_ELEMENT_ID = 'kAddCurrentTabToReadingListElementId';
const READING_LIST_UNREAD_ELEMENT_ID = 'kSidePanelReadingListUnreadElementId';
const MARKED_AS_READ_NATIVE_EVENT_ID = 'kSidePanelReadingMarkedAsReadEventId';

export class ReadingListAppElement extends ReadingListAppElementBase {
  static get is() {
    return 'reading-list-app';
  }

  static get template() {
    return getTemplate();
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
  private apiProxy_: ReadingListApiProxy =
      ReadingListApiProxyImpl.getInstance();
  private listenerIds_: number[] = [];
  private visibilityChangedListener_: () => void;
  private readingListEventTracker_: EventTracker = new EventTracker();

  constructor() {
    super();

    this.visibilityChangedListener_ = () => {
      // Refresh Reading List's list data when transitioning into a visible
      // state.
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

    // If added in a visible state update current reading list items.
    // If UnifiedSidePanel is enabled do this immediately. This is only
    // undesirable when UnifiedSidePanel is not enabled since previously reading
    // list and bookmarks shared a webview.
    if (document.visibilityState === 'visible' || this.unifiedSidePanel_) {
      this.updateReadLaterEntries_();
      this.apiProxy_.updateCurrentPageActionButtonState();
    }

    this.readingListEventTracker_.add(
        this.root!, MARKED_AS_READ_UI_EVENT, this.onMarkedAsRead.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));

    document.removeEventListener(
        'visibilitychange', this.visibilityChangedListener_);

    this.unregisterHelpBubble(READING_LIST_UNREAD_ELEMENT_ID);

    this.readingListEventTracker_.remove(this.root!, MARKED_AS_READ_UI_EVENT);
  }

  override ready() {
    super.ready();

    this.registerHelpBubble(
        ADD_CURRENT_TAB_ELEMENT_ID, '#currentPageActionButton');

    this.$.unreadItemsList.addEventListener(
        'rendered-item-count-changed', () => {
          const firstUnreadItem =
              this.root!.querySelector('.unread-item') as HTMLElement | null;
          if (firstUnreadItem) {
            this.registerHelpBubble(
                READING_LIST_UNREAD_ELEMENT_ID, firstUnreadItem);
          }
        });
  }

  /**
   * Fetches the latest reading list entries from the browser.
   */
  private async updateReadLaterEntries_() {
    const getEntriesStartTimestamp = Date.now();

    const {entries} = await this.apiProxy_.getReadLaterEntries();

    chrome.metricsPrivate.recordTime(
        'ReadingList.WebUI.ReadingListDataReceived',
        Math.round(Date.now() - getEntriesStartTimestamp));

    if (entries.unreadEntries.length !== 0 ||
        entries.readEntries.length !== 0) {
      listenOnce(this.$.readingListList, 'dom-change', () => {
        // Push ShowUI() callback to the event queue to allow deferred rendering
        // to take place.
        setTimeout(() => this.apiProxy_.showUi(), 0);
      });
    } else {
      setTimeout(() => this.apiProxy_.showUi(), 0);
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

  /**
   * @return The appropriate text for the empty state subheader
   */
  private getEmptyStateSubheaderText_(): string {
    return loadTimeData.getString('emptyStateAddFromDialogSubheader');
  }

  /**
   * @return The appropriate text for the current page action button
   */
  private getCurrentPageActionButtonText_(): string {
    if (this.getCurrentPageActionButtonMarkAsRead_()) {
      return loadTimeData.getString('markCurrentTabAsRead');
    } else {
      return loadTimeData.getString('addCurrentTab');
    }
  }

  /**
   * @return The appropriate cr icon for the current page action button
   */
  private getCurrentPageActionButtonIcon_(): string {
    if (this.getCurrentPageActionButtonMarkAsRead_()) {
      return 'cr:check';
    } else {
      return 'cr:add';
    }
  }

  /**
   * @return Whether the current page action button should be disabled
   */
  private getCurrentPageActionButtonDisabled_(): boolean {
    return this.currentPageActionButtonState_ ===
        CurrentPageActionButtonState.kDisabled;
  }

  /**
   * @return Whether the current page action button should be in its mark as
   * read state
   */
  private getCurrentPageActionButtonMarkAsRead_(): boolean {
    return this.currentPageActionButtonState_ ===
        CurrentPageActionButtonState.kMarkAsRead;
  }

  private isReadingListEmpty_(): boolean {
    return this.unreadItems_.length === 0 && this.readItems_.length === 0;
  }

  private onCurrentPageActionButtonClick_() {
    if (this.getCurrentPageActionButtonMarkAsRead_()) {
      this.apiProxy_.markCurrentTabAsRead();
      this.sendTutorialCustomEvent();
    } else {
      this.apiProxy_.addCurrentTab();
    }
  }

  private onMarkedAsRead() {
    this.sendTutorialCustomEvent();
  }

  private sendTutorialCustomEvent() {
    this.notifyHelpBubbleAnchorCustomEvent(
        READING_LIST_UNREAD_ELEMENT_ID,
        MARKED_AS_READ_NATIVE_EVENT_ID,
    );
  }


  private onItemKeyDown_(e: KeyboardEvent) {
    if (e.shiftKey || !navigationKeys.has(e.key)) {
      return;
    }
    switch (e.key) {
      case 'ArrowDown':
        this.$.selector.selectNext();
        (this.$.selector.selectedItem as ReadingListItemElement).focus();
        break;
      case 'ArrowUp':
        this.$.selector.selectPrevious();
        (this.$.selector.selectedItem as ReadingListItemElement).focus();
        break;
      default:
        assertNotReached();
    }
    e.preventDefault();
    e.stopPropagation();
  }

  private onItemFocus_(e: Event) {
    this.$.selector.selected =
        (e.currentTarget as ReadingListItemElement).dataset.url!;
  }

  private shouldShowHr_(): boolean {
    return this.unreadItems_.length > 0 && this.readItems_.length > 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'reading-list-app': ReadingListAppElement;
  }
}

customElements.define(ReadingListAppElement.is, ReadingListAppElement);
