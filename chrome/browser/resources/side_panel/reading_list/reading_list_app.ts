// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://read-later.top-chrome/shared/sp_empty_state.js';
import 'chrome://read-later.top-chrome/shared/sp_footer.js';
import 'chrome://read-later.top-chrome/shared/sp_heading.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_lazy_list/cr_lazy_list.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './reading_list_item.js';
import '/strings.m.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
import type {CrLazyListElement} from 'chrome://resources/cr_elements/cr_lazy_list/cr_lazy_list.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ReadLaterEntriesByStatus, ReadLaterEntry} from './reading_list.mojom-webui.js';
import {CurrentPageActionButtonState} from './reading_list.mojom-webui.js';
import type {ReadingListApiProxy} from './reading_list_api_proxy.js';
import {ReadingListApiProxyImpl} from './reading_list_api_proxy.js';
import {getCss} from './reading_list_app.css.js';
import {getHtml} from './reading_list_app.html.js';
import {MARKED_AS_READ_UI_EVENT} from './reading_list_item.js';

const navigationKeys: Set<string> = new Set(['ArrowDown', 'ArrowUp']);

const ReadingListAppElementBase = HelpBubbleMixinLit(CrLitElement);

export interface ReadingListAppElement {
  $: {
    footer: HTMLElement,
    readingListList: CrLazyListElement,
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
      focusedIndex_: {type: Number},
      focusedItem_: {type: Object},
      currentPageActionButtonState_: {type: Number},
      buttonRipples: {type: Boolean},
      loadingContent_: {type: Boolean},
      itemSize_: {type: Number},
      minViewportHeight_: {type: Number},
      scrollTarget_: {type: Object},
      unreadHeader_: {type: String},
      readHeader_: {type: String},
      unreadExpanded_: {type: Boolean},
      readExpanded_: {type: Boolean},
      isWebUIBrowser_: {type: Boolean},
    };
  }

  protected accessor unreadItems_: ReadLaterEntry[] = [];
  protected accessor readItems_: ReadLaterEntry[] = [];
  protected accessor focusedIndex_: number = -1;
  protected accessor focusedItem_: HTMLElement|null = null;
  private accessor currentPageActionButtonState_: CurrentPageActionButtonState =
      CurrentPageActionButtonState.kDisabled;
  accessor buttonRipples: boolean = loadTimeData.getBoolean('useRipples');
  protected accessor loadingContent_: boolean = true;
  protected accessor itemSize_: number = 48;
  protected accessor minViewportHeight_: number = 0;
  protected accessor scrollTarget_: HTMLElement|null = null;
  private accessor unreadHeader_: string =
      loadTimeData.getString('unreadHeader');
  private accessor readHeader_: string = loadTimeData.getString('readHeader');
  private accessor unreadExpanded_: boolean = true;
  private accessor readExpanded_: boolean = false;
  private accessor isWebUIBrowser_: boolean =
      loadTimeData.getBoolean('isWebUIBrowser');
  private apiProxy_: ReadingListApiProxy =
      ReadingListApiProxyImpl.getInstance();
  private listenerIds_: number[] = [];
  private visibilityChangedListener_: () => void;
  private readingListEventTracker_: EventTracker = new EventTracker();

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();

    this.visibilityChangedListener_ = () => {
      // Refresh Reading List's list data when transitioning into a visible
      // state.
      if (document.visibilityState === 'visible') {
        this.updateReadLaterEntries_();
        this.updateViewportHeight_();
      }
    };
  }

  override connectedCallback() {
    super.connectedCallback();

    if (this.isWebUIBrowser_) {
      this.classList.add('in-webui-browser');
    }

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

    this.scrollTarget_ = this.$.readingListList;
    this.updateReadLaterEntries_();
    this.updateViewportHeight_();
    this.apiProxy_.updateCurrentPageActionButtonState();

    this.readingListEventTracker_.add(
        this.shadowRoot, MARKED_AS_READ_UI_EVENT,
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
        this.shadowRoot, MARKED_AS_READ_UI_EVENT);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('unreadItems_') ||
        changedPrivateProperties.has('readItems_')) {
      const listLength = this.getAllItems_().length;
      if (this.focusedIndex_ >= listLength) {
        this.focusedIndex_ = listLength - 1;
      } else if (this.focusedIndex_ === -1 && listLength > 0) {
        this.focusedIndex_ = 1;
      }
    }
  }

  override firstUpdated() {
    this.registerHelpBubble(
        ADD_CURRENT_TAB_ELEMENT_ID, '#currentPageActionButton');

    const firstUnreadItem =
        this.shadowRoot.querySelector<HTMLElement>('.unread-item');
    if (firstUnreadItem) {
      this.registerHelpBubble(READING_LIST_UNREAD_ELEMENT_ID, firstUnreadItem);
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('focusedIndex_')) {
      this.updateFocusedItem_();
    }
  }

  private updateViewportHeight_() {
    this.apiProxy_.getWindowData().then(({windows}) => {
      const activeWindow = windows.find((w) => w.active);
      const windowHeight =
          activeWindow ? activeWindow.height : windows[0]!.height;
      this.minViewportHeight_ = windowHeight - this.$.footer.offsetHeight;
    });
  }

  getFocusedIndexForTesting() {
    return this.focusedIndex_;
  }

  setExpandedForTesting() {
    this.readExpanded_ = true;
    this.unreadExpanded_ = true;
  }

  protected updateFocusedItem_() {
    this.focusedItem_ = this.focusedIndex_ === -1 ?
        null :
        this.querySelector<HTMLElement>(
            `cr-lazy-list > *:nth-child(${this.focusedIndex_ + 1})`);
  }

  protected getAllItems_(): ReadLaterEntry[] {
    const allItems: ReadLaterEntry[] = [];
    if (this.unreadItems_.length > 0) {
      allItems.push(this.createHeaderEntry_(this.unreadHeader_));
      if (this.unreadExpanded_) {
        allItems.push(...this.unreadItems_);
      }
    }
    if (this.readItems_.length > 0) {
      allItems.push(this.createHeaderEntry_(this.readHeader_));
      if (this.readExpanded_) {
        allItems.push(...this.readItems_);
      }
    }
    return allItems;
  }

  private createHeaderEntry_(title: string): ReadLaterEntry {
    return {
      title: title,
      url: {url: ''},
      displayUrl: '',
      updateTime: 0n,
      read: false,
      displayTimeSinceUpdate: '',
    };
  }

  protected getExpandButtonAriaLabel_(title: string): string {
    let labelString: string;
    switch (title) {
      case this.unreadHeader_:
        labelString = this.unreadExpanded_ ? 'collapseButtonAriaLabel' :
                                             'expandButtonAriaLabel';
        break;
      case this.readHeader_:
        labelString = this.readExpanded_ ? 'collapseButtonAriaLabel' :
                                           'expandButtonAriaLabel';
        break;
      default:
        assertNotReached();
    }
    return loadTimeData.getStringF(labelString, title);
  }

  protected getExpandButtonIcon_(title: string): string {
    switch (title) {
      case this.unreadHeader_:
        return this.unreadExpanded_ ? 'cr:expand-less' : 'cr:expand-more';
      case this.readHeader_:
        return this.readExpanded_ ? 'cr:expand-less' : 'cr:expand-more';
      default:
        assertNotReached();
    }
  }

  protected onExpandButtonClick_(e: Event) {
    const title: string = (e.target as HTMLElement).dataset['title']!;
    switch (title) {
      case this.unreadHeader_:
        this.unreadExpanded_ = !this.unreadExpanded_;
        break;
      case this.readHeader_:
        this.readExpanded_ = !this.readExpanded_;
        break;
      default:
        assertNotReached();
    }
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

  private updateReadingListItems_(entries: ReadLaterEntriesByStatus) {
    this.unreadItems_ = entries.unreadEntries;
    this.readItems_ = entries.readEntries;
    this.loadingContent_ = false;
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
    e.stopPropagation();
    e.preventDefault();

    // Skip focus traversal for the header of the first section.
    const indicesToSkip = [0];
    if (this.unreadItems_.length > 0 && this.readItems_.length > 0) {
      // If both sections are shown, skip the header of the second section too.
      if (this.unreadExpanded_) {
        indicesToSkip.push(this.unreadItems_.length + 1);
      } else {
        indicesToSkip.push(1);
      }
    }
    const listLength = this.getAllItems_().length;

    // Identify the new focused index.
    let newFocusedIndex = 0;
    if (e.key === 'ArrowUp') {
      newFocusedIndex = (this.focusedIndex_ + listLength - 1) % listLength;
      while (indicesToSkip.includes(newFocusedIndex)) {
        newFocusedIndex = (newFocusedIndex + listLength - 1) % listLength;
      }
    } else {
      newFocusedIndex = (this.focusedIndex_ + 1) % listLength;
      while (indicesToSkip.includes(newFocusedIndex)) {
        newFocusedIndex = (newFocusedIndex + 1) % listLength;
      }
    }
    this.focusedIndex_ = newFocusedIndex;

    const element =
        await this.$.readingListList.ensureItemRendered(this.focusedIndex_);
    element.focus();
  }

  protected onItemFocus_(e: Event) {
    const renderedItems =
        this.querySelectorAll<HTMLElement>('cr-lazy-list > *');
    const focusedIdx = Array.from(renderedItems).findIndex(item => {
      return item === e.target || item.shadowRoot?.activeElement === e.target;
    });

    if (focusedIdx !== -1) {
      this.focusedIndex_ = focusedIdx;
    }
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
