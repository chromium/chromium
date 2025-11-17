// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './searched_label.js';
import './shared_icons.html.js';
import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';

import {HistoryResultType} from 'chrome://resources/cr_components/history/constants.js';
import type {HistoryEntry} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {FocusRowMixinLit} from 'chrome://resources/cr_elements/focus_row_mixin_lit.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {BrowserServiceImpl} from './browser_service.js';
import {getCss} from './history_item.css.js';
import {getHtml} from './history_item.html.js';

export interface HistoryItemElement {
  $: {
    'checkbox': CrCheckboxElement,
    'icon': HTMLElement,
    'link': HTMLElement,
    'menu-button': CrIconButtonElement,
    'time-accessed': HTMLElement,
  };
}

const HistoryItemElementBase = FocusRowMixinLit(CrLitElement);

export class HistoryItemElement extends HistoryItemElementBase {
  static get is() {
    return 'history-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // Underlying HistoryEntry data for this.item. Contains read-only fields
      // from the history backend, as well as fields computed by history-list.
      item: {type: Object},

      selected: {
        type: Boolean,
        reflect: true,
      },

      isCardStart: {
        type: Boolean,
        reflect: true,
      },

      isCardEnd: {
        type: Boolean,
        reflect: true,
      },

      selectionNotAllowed_: {type: Boolean},

      hasTimeGap: {type: Boolean},

      index: {type: Number},

      numberOfItems: {type: Number},

      // Search term used to obtain this history-item.
      searchTerm: {type: String},
    };
  }

  private isShiftKeyDown_: boolean = false;
  protected accessor selectionNotAllowed_: boolean =
      !loadTimeData.getBoolean('allowDeletingHistory');
  private eventTracker_: EventTracker = new EventTracker();
  accessor item: HistoryEntry|undefined;
  accessor hasTimeGap: boolean = false;
  accessor index: number = -1;
  accessor searchTerm: string = '';
  accessor isCardStart: boolean = false;
  accessor isCardEnd: boolean = false;
  accessor numberOfItems: number = 0;
  accessor selected: boolean = false;

  override connectedCallback() {
    super.connectedCallback();

    this.updateComplete.then(() => {
      // Adding listeners asynchronously to reduce blocking time, since these
      // history items are items in a potentially long list.
      this.eventTracker_.add(
          this.$.checkbox, 'keydown',
          (e: Event) => this.onCheckboxKeydown_(e as KeyboardEvent));
    });
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('item')) {
      this.itemChanged_();
      this.fire('iron-resize');
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.remove(this.$.checkbox, 'keydown');
  }

  focusOnMenuButton() {
    focusWithoutInk(this.$['menu-button']);
  }

  private onCheckboxKeydown_(e: KeyboardEvent) {
    if (e.shiftKey && e.key === 'Tab') {
      this.focus();
    }
  }

  /**
   * Toggle item selection whenever the checkbox or any non-interactive part
   * of the item is clicked.
   */
  protected onRowClick_(e: MouseEvent) {
    const path = e.composedPath();
    // VoiceOver has issues with click events within elements that have a role
    // of row, so this event listeners has to be on the row itself.
    // (See crbug.com/1185827.)
    let inItemContainer = false;
    for (let i = 0; i < path.length; i++) {
      const elem = path[i] as HTMLElement;
      if (elem.id !== 'checkbox' &&
          (elem.nodeName === 'A' || elem.nodeName === 'CR-ICON-BUTTON')) {
        return;
      }

      if (!inItemContainer && elem.id === 'item-container') {
        inItemContainer = true;
      }
    }

    if (this.selectionNotAllowed_ || !inItemContainer) {
      return;
    }

    this.$.checkbox.focus();
    this.fire('history-checkbox-select', {
      index: this.index,
      shiftKey: e.shiftKey,
    });
  }

  /**
   * This is bound to mouse/keydown instead of click/press because this
   * has to fire before onCheckboxChange_. If we bind it to click/press,
   * it might trigger out of desired order.
   */
  protected onCheckboxClick_(e: MouseEvent) {
    this.isShiftKeyDown_ = e.shiftKey;
  }

  protected onCheckboxChange_() {
    this.fire('history-checkbox-select', {
      index: this.index,
      // If the user clicks or press enter/space key, oncheckboxClick_ will
      // trigger before this function, so a shift-key might be recorded.
      shiftKey: this.isShiftKeyDown_,
    });

    this.isShiftKeyDown_ = false;
  }

  protected onRowMousedown_(e: MouseEvent) {
    // Prevent shift clicking a checkbox from selecting text.
    if (e.shiftKey) {
      e.preventDefault();
    }
  }

  protected getEntrySummary_(): string {
    const item = this.item;
    if (!item) {
      return '';
    }
    return loadTimeData.getStringF(
        'entrySummary',
        this.isCardStart || this.isCardEnd ? this.cardTitle_() : '',
        item.dateTimeOfDay,
        item.starred ? loadTimeData.getString('bookmarked') : '', item.title,
        item.domain);
  }

  /**
   * The first and last rows of a card have a described-by field pointing to
   * the date header, to make sure users know if they have jumped between cards
   * when navigating up or down with the keyboard.
   */
  protected getAriaDescribedByForHeading_(): string {
    return this.isCardStart || this.isCardEnd ? 'date-accessed' : '';
  }

  /**
   * Actions menu is described by the title and domain of the row and may
   * include the date to make sure users know if they have jumped between dates.
   */
  protected getAriaDescribedByForActions_(): string {
    return this.isCardStart || this.isCardEnd ?
        'title-and-domain date-accessed' :
        'title-and-domain';
  }

  protected shouldShowActorTooltip_() {
    return loadTimeData.getBoolean('enableBrowsingHistoryActorIntegrationM1') &&
        this.item?.isActorVisit;
  }

  /**
   * Remove bookmark of current item when bookmark-star is clicked.
   */
  protected onRemoveBookmarkClick_() {
    if (!this.item?.starred) {
      return;
    }

    if (this.shadowRoot.querySelector('#bookmark-star') ===
        this.shadowRoot.activeElement) {
      focusWithoutInk(this.$['menu-button']);
    }

    const browserService = BrowserServiceImpl.getInstance();
    browserService.handler.removeBookmark(this.item.url);
    browserService.recordAction('BookmarkStarClicked');

    this.fire('remove-bookmark-stars', this.item.url);
  }

  /**
   * Fires a custom event when the menu button is clicked. Sends the details
   * of the history item and where the menu should appear.
   */
  protected onMenuButtonClick_(e: Event) {
    this.fire('open-menu', {
      target: e.target,
      index: this.index,
      item: this.item,
    });

    // Stops the 'click' event from closing the menu when it opens.
    e.stopPropagation();
  }

  protected onMenuButtonKeydown_(e: KeyboardEvent) {
    if (this.item?.starred && e.shiftKey && e.key === 'Tab') {
      // If this item has a bookmark star, pressing shift + Tab from the more
      // actions menu should move focus to the star. FocusRow will try to
      // instead move focus to the previous focus row control, and since the
      // star is not a focus row control, stop immediate propagation here to
      // instead allow default browser behavior.
      e.stopImmediatePropagation();
    }
  }

  /**
   * Record metrics when a result is clicked.
   */
  protected onLinkClick_() {
    const browserService = BrowserServiceImpl.getInstance();
    browserService.recordAction('EntryLinkClick');

    if (this.searchTerm) {
      browserService.recordAction('SearchResultClick');
    }

    this.fire('record-history-link-click', {
      resultType: HistoryResultType.TRADITIONAL,
      index: this.index,
    });
  }

  protected onLinkRightClick_() {
    BrowserServiceImpl.getInstance().recordAction('EntryLinkRightClick');
  }

  /**
   * Set the favicon image, based on the URL of the history item.
   */
  private itemChanged_() {
    if (!this.item) {
      return;
    }
    this.$.icon.style.backgroundImage = getFaviconForPageURL(
        this.item.url, this.item.isUrlInRemoteUserData,
        this.item.remoteIconUrlForUma);
    this.eventTracker_.add(
        this.$['time-accessed'], 'mouseover', () => this.addTimeTitle_());
  }

  protected cardTitle_(): string {
    if (this.item === undefined) {
      return '';
    }

    if (!this.searchTerm) {
      return this.item.dateRelativeDay;
    }
    return searchResultsTitle(this.numberOfItems, this.searchTerm);
  }

  private addTimeTitle_() {
    if (!this.item) {
      return;
    }
    const el = this.$['time-accessed'];
    el.setAttribute('title', new Date(this.item.time).toString());
    this.eventTracker_.remove(el, 'mouseover');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-item': HistoryItemElement;
  }
}

customElements.define(HistoryItemElement.is, HistoryItemElement);

/**
 * @return The title for a page of search results.
 */
export function searchResultsTitle(
    numberOfResults: number, searchTerm: string): string {
  const resultId = numberOfResults === 1 ? 'searchResult' : 'searchResults';
  return loadTimeData.getStringF(
      'foundSearchResults', numberOfResults, loadTimeData.getString(resultId),
      searchTerm);
}
