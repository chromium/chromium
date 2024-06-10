// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://read-later.top-chrome/shared/sp_empty_state.js';
import 'chrome://read-later.top-chrome/shared/sp_footer.js';
import 'chrome://read-later.top-chrome/shared/sp_heading.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import './reading_list_item.js';
import '../strings.m.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import {CrSelectableMixin} from 'chrome://resources/cr_elements/cr_selectable_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ReadLaterEntriesByStatus, ReadLaterEntry} from './reading_list.mojom-webui.js';
import {CurrentPageActionButtonState} from './reading_list.mojom-webui.js';
import type {ReadingListApiProxy} from './reading_list_api_proxy.js';
import {ReadingListApiProxyImpl} from './reading_list_api_proxy.js';
import {getCss} from './reading_list_app.css.js';
import {getHtml} from './reading_list_app.html.js';
import type {ReadingListItemElement} from './reading_list_item.js';
import {MARKED_AS_READ_UI_EVENT} from './reading_list_item.js';

const navigationKeys: Set<string> = new Set(['ArrowDown', 'ArrowUp']);

const ReadingListAppElementBase =
    HelpBubbleMixinLit(CrSelectableMixin(CrLitElement));

export interface ReadingListAppElement {
  $: {
    readingListList: HTMLElement,
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

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      unreadItems_: {type: Array},
      readItems_: {type: Array},
      currentPageActionButtonState_: {type: Number},
      buttonRipples: {type: Boolean},
      loadingContent_: {type: Boolean},
    };
  }

  protected unreadItems_: ReadLaterEntry[] = [];
  protected readItems_: ReadLaterEntry[] = [];
  private currentPageActionButtonState_: CurrentPageActionButtonState =
      CurrentPageActionButtonState.kDisabled;
  buttonRipples: boolean = loadTimeData.getBoolean('useRipples');
  protected loadingContent_: boolean = true;
  private apiProxy_: ReadingListApiProxy =
      ReadingListApiProxyImpl.getInstance();
  private listenerIds_: number[] = [];
  private visibilityChangedListener_: () => void;
  private readingListEventTracker_: EventTracker = new EventTracker();

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();

    /** Property for CrSelectableMixin */
    this.attrForSelected = 'data-url';

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
            (entries: ReadLaterEntriesByStatus) =>
                this.updateReadingListItems_(entries)),
        callbackRouter.currentPageActionButtonStateChanged.addListener(
            (state: CurrentPageActionButtonState) =>
                this.updateCurrentPageActionButton_(state)));

    this.updateReadLaterEntries_();
    this.apiProxy_.updateCurrentPageActionButtonState();

    this.readingListEventTracker_.add(
        this.shadowRoot!, MARKED_AS_READ_UI_EVENT,
        this.onMarkedAsRead_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => this.apiProxy_.getCallbackRouter().removeListener(id));

    document.removeEventListener(
        'visibilitychange', this.visibilityChangedListener_);

    this.unregisterHelpBubble(READING_LIST_UNREAD_ELEMENT_ID);

    this.readingListEventTracker_.remove(
        this.shadowRoot!, MARKED_AS_READ_UI_EVENT);
  }

  override firstUpdated() {
    this.registerHelpBubble(
        ADD_CURRENT_TAB_ELEMENT_ID, '#currentPageActionButton');

    const firstUnreadItem =
        this.shadowRoot!.querySelector<HTMLElement>('.unread-item');
    if (firstUnreadItem) {
      this.registerHelpBubble(READING_LIST_UNREAD_ELEMENT_ID, firstUnreadItem);
    }
  }

  // Override `observeItems` from CrSelectableMixin.
  override observeItems() {
    // Turn off default observation logic in CrSelectableMixin.
  }

  // Override `queryItems` from CrSelectableMixin.
  override queryItems() {
    return Array.from(this.shadowRoot!.querySelectorAll('reading-list-item'));
  }

  // Override `queryMatchingItem` from CrSelectableMixin.
  override queryMatchingItem(selector: string) {
    return this.shadowRoot!.querySelector<HTMLElement>(
        `reading-list-item${selector}`);
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

    // Push ShowUI() callback to the event queue to allow deferred rendering
    // to take place.
    setTimeout(() => this.apiProxy_.showUi(), 0);
    this.updateReadingListItems_(entries);
  }

  private async updateReadingListItems_(entries: ReadLaterEntriesByStatus) {
    this.unreadItems_ = entries.unreadEntries;
    this.readItems_ = entries.readEntries;
    this.loadingContent_ = false;

    await this.updateComplete;
    this.itemsChanged();
  }

  private updateCurrentPageActionButton_(state: CurrentPageActionButtonState) {
    this.currentPageActionButtonState_ = state;
  }

  protected ariaLabel_(item: ReadLaterEntry): string {
    return `${item.title} - ${item.displayUrl} - ${
        item.displayTimeSinceUpdate}`;
  }

  /**
   * @return The appropriate text for the empty state subheader
   */
  protected getEmptyStateSubheaderText_(): string {
    return loadTimeData.getString('emptyStateAddFromDialogSubheader');
  }

  /**
   * @return The appropriate text for the current page action button
   */
  protected getCurrentPageActionButtonText_(): string {
    return loadTimeData.getString(
        this.getCurrentPageActionButtonMarkAsRead_() ? 'markCurrentTabAsRead' :
                                                       'addCurrentTab');
  }

  /**
   * @return The appropriate cr icon for the current page action button
   */
  protected getCurrentPageActionButtonIcon_(): string {
    return this.getCurrentPageActionButtonMarkAsRead_() ? 'cr:check' : 'cr:add';
  }

  /**
   * @return Whether the current page action button should be disabled
   */
  protected getCurrentPageActionButtonDisabled_(): boolean {
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

  protected isReadingListEmpty_(): boolean {
    return this.unreadItems_.length === 0 && this.readItems_.length === 0;
  }

  protected onCurrentPageActionButtonClick_() {
    if (this.getCurrentPageActionButtonMarkAsRead_()) {
      this.apiProxy_.markCurrentTabAsRead();
      this.sendTutorialCustomEvent();
    } else {
      this.apiProxy_.addCurrentTab();
    }
  }

  private onMarkedAsRead_() {
    this.sendTutorialCustomEvent();
  }

  private sendTutorialCustomEvent() {
    this.notifyHelpBubbleAnchorCustomEvent(
        READING_LIST_UNREAD_ELEMENT_ID,
        MARKED_AS_READ_NATIVE_EVENT_ID,
    );
  }


  protected async onItemKeyDown_(e: KeyboardEvent) {
    if (e.shiftKey || !navigationKeys.has(e.key)) {
      return;
    }
    switch (e.key) {
      case 'ArrowDown':
        this.selectNext();
        await this.updateComplete;
        (this.selectedItem as ReadingListItemElement).focus();
        break;
      case 'ArrowUp':
        this.selectPrevious();
        await this.updateComplete;
        (this.selectedItem as ReadingListItemElement).focus();
        break;
      default:
        assertNotReached();
    }
    e.preventDefault();
    e.stopPropagation();
  }

  protected onItemFocus_(e: Event) {
    this.selected = (e.currentTarget as ReadingListItemElement).dataset['url']!;
  }

  protected shouldShowHr_(): boolean {
    return this.unreadItems_.length > 0 && this.readItems_.length > 0;
  }

  protected shouldShowList_(): boolean {
    return this.unreadItems_.length > 0 || this.readItems_.length > 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'reading-list-app': ReadingListAppElement;
  }
}

customElements.define(ReadingListAppElement.is, ReadingListAppElement);
