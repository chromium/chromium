// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import type {CrUrlListItemElement} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {CrUrlListItemSize} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getCss} from './power_bookmark_row.css.js';
import {getHtml} from './power_bookmark_row.html.js';
import type {PowerBookmarksService} from './power_bookmarks_service.js';

export interface PowerBookmarkRowElement {
  $: {
    crUrlListItem: CrUrlListItemElement,
  };
}

export class PowerBookmarkRowElement extends CrLitElement {
  static get is() {
    return 'power-bookmark-row';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      bookmark: {type: Object},
      checkboxChecked: {type: Boolean},
      checkboxDisabled: {type: Boolean},
      compact: {type: Boolean},
      description: {type: String},
      descriptionMeta: {type: String},
      bookmarksTreeViewEnabled: {type: Boolean},
      forceHover: {type: Boolean},
      hasCheckbox: {
        type: Boolean,
        reflect: true,
      },
      hasInput: {
        type: Boolean,
        reflect: true,
      },
      imageUrls: {type: Array},
      isShoppingCollection: {type: Boolean},
      rowAriaDescription: {type: String},
      rowAriaLabel: {type: String},
      trailingIcon: {type: String},
      trailingIconAriaLabel: {type: String},
      trailingIconTooltip: {type: String},
      listItemSize: {type: String},
      bookmarksService: {type: Object},
    };
  }

  bookmark: chrome.bookmarks.BookmarkTreeNode;
  checkboxChecked: boolean = false;
  checkboxDisabled: boolean = false;
  compact: boolean = false;
  description: string = '';
  descriptionMeta: string = '';
  bookmarksTreeViewEnabled: boolean =
      loadTimeData.getBoolean('bookmarksTreeViewEnabled');
  forceHover: boolean = false;
  hasCheckbox: boolean = false;
  hasInput: boolean = false;
  isShoppingCollection: boolean = false;
  rowAriaDescription: string = '';
  rowAriaLabel: string = '';
  trailingIcon: string = '';
  trailingIconAriaLabel: string = '';
  trailingIconTooltip: string = '';
  imageUrls: string[] = [];

  listItemSize: CrUrlListItemSize = CrUrlListItemSize.COMPACT;
  bookmarksService: PowerBookmarksService;

  override connectedCallback() {
    super.connectedCallback();
    this.onInputDisplayChange_();
    this.addEventListener('keydown', this.onKeydown_);
    this.addEventListener('focus', this.onFocus_);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('compact')) {
      this.listItemSize =
          this.compact ? CrUrlListItemSize.COMPACT : CrUrlListItemSize.LARGE;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('listItemSize')) {
      this.onListItemSizeChanged_();
    }
    if (changedProperties.has('hasInput') ||
        changedProperties.has('hasCheckbox')) {
      this.onInputDisplayChange_();
    }
  }

  override focus() {
    this.$.crUrlListItem.focus();
  }

  private onKeydown_(e: KeyboardEvent) {
    if (this.shadowRoot!.activeElement !== this.$.crUrlListItem) {
      return;
    }
    if (e.shiftKey && e.key === 'Tab') {
      // Hitting shift tab from CrUrlListItem to traverse focus backwards will
      // attempt to move focus to this element, which is responsible for
      // delegating focus but should itself not be focusable. So when the user
      // hits shift tab, immediately hijack focus onto itself so that the
      // browser moves focus to the focusable element before it once it
      // processes the shift tab.
      super.focus();
    } else if (e.key === 'Enter') {
      // Prevent iron-list from moving focus.
      e.stopPropagation();
    }
  }

  private onFocus_(e: FocusEvent) {
    if (e.composedPath()[0] === this && this.matches(':focus-visible')) {
      // If trying to directly focus on this row, move the focus to the
      // <cr-url-list-item>. Otherwise, UI might be trying to directly focus on
      // a specific child (eg. the input).
      // This should only be done when focusing via keyboard, to avoid blocking
      // drag interactions.
      this.$.crUrlListItem.focus();
    }
  }

  private async onListItemSizeChanged_() {
    await this.$.crUrlListItem.updateComplete;
    if (this.parentNode &&
        (this.parentNode as HTMLElement).tagName === 'IRON-LIST') {
      this.parentNode.dispatchEvent(new CustomEvent('iron-resize'));
    }
  }

  protected isBookmarksBar_(): boolean {
    return this.bookmark?.id === loadTimeData.getString('bookmarksBarId');
  }

  protected showTrailingIcon_(): boolean {
    return !this.hasInput && !this.hasCheckbox;
  }

  private onInputDisplayChange_() {
    const input = this.shadowRoot!.querySelector<CrInputElement>('#input');
    if (input) {
      input.select();
    }
  }

  /**
   * Dispatches a custom click event when the user clicks anywhere on the row.
   */
  protected onRowClicked_(event: MouseEvent) {
    // Ignore clicks on the row when it has an input, to ensure the row doesn't
    // eat input clicks. Also ignore clicks if the row has no associated
    // bookmark, or if the event is a right-click.
    if (this.hasInput || !this.bookmark || event.button === 2) {
      return;
    }
    event.preventDefault();
    event.stopPropagation();
    if (this.hasCheckbox && !this.checkboxDisabled) {
      // Clicking the row should trigger a checkbox click rather than a
      // standard row click.
      const checkbox =
          this.shadowRoot!.querySelector<CrCheckboxElement>('#checkbox')!;
      checkbox.checked = !checkbox.checked;
      return;
    }
    this.dispatchEvent(new CustomEvent('row-clicked', {
      bubbles: true,
      composed: true,
      detail: {
        bookmark: this.bookmark,
        event: event,
      },
    }));
  }

  /**
   * Dispatches a custom click event when the user right-clicks anywhere on the
   * row.
   */
  protected onContextMenu_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.dispatchEvent(new CustomEvent('context-menu', {
      bubbles: true,
      composed: true,
      detail: {
        bookmark: this.bookmark,
        event: event,
      },
    }));
  }

  /**
   * Dispatches a custom click event when the user clicks anywhere on the
   * trailing icon button.
   */
  protected onTrailingIconClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.dispatchEvent(new CustomEvent('trailing-icon-clicked', {
      bubbles: true,
      composed: true,
      detail: {
        bookmark: this.bookmark,
        event: event,
      },
    }));
  }

  /**
   * Dispatches a custom click event when the user clicks on the checkbox.
   */
  protected onCheckboxChange_(event: Event) {
    event.preventDefault();
    event.stopPropagation();
    this.dispatchEvent(new CustomEvent('checkbox-change', {
      bubbles: true,
      composed: true,
      detail: {
        bookmark: this.bookmark,
        checked: (event.target as CrCheckboxElement).checked,
      },
    }));
  }

  /**
   * Triggers an input change event on enter. Extends default input behavior
   * which only triggers a change event if the value of the input has changed.
   */
  protected onInputKeyDown_(event: KeyboardEvent) {
    if (event.key === 'Enter') {
      event.stopPropagation();
      this.onInputChange_(event);
    }
  }

  private createInputChangeEvent_(value: string|null) {
    return new CustomEvent('input-change', {
      bubbles: true,
      composed: true,
      detail: {
        bookmark: this.bookmark,
        value: value,
      },
    });
  }

  /**
   * Triggers a custom input change event when the user hits enter or the input
   * loses focus.
   */
  protected onInputChange_(event: Event) {
    event.preventDefault();
    event.stopPropagation();
    const inputElement =
        this.shadowRoot!.querySelector<CrInputElement>('#input')!;
    this.dispatchEvent(this.createInputChangeEvent_(inputElement.value));
  }

  protected onInputBlur_(event: Event) {
    event.preventDefault();
    event.stopPropagation();
    this.dispatchEvent(this.createInputChangeEvent_(null));
  }

  protected isPriceTracked_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      boolean {
    return !!this.bookmarksService?.getPriceTrackedInfo(bookmark);
  }

  /**
   * Whether the given price-tracked bookmark should display as if discounted.
   */
  protected showDiscountedPrice_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      boolean {
    const bookmarkProductInfo =
        this.bookmarksService?.getPriceTrackedInfo(bookmark);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.previousPrice.length > 0;
    }
    return false;
  }

  protected getCurrentPrice_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      string {
    const bookmarkProductInfo =
        this.bookmarksService?.getPriceTrackedInfo(bookmark);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.currentPrice;
    } else {
      return '';
    }
  }

  protected getPreviousPrice_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      string {
    const bookmarkProductInfo =
        this.bookmarksService?.getPriceTrackedInfo(bookmark);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.previousPrice;
    } else {
      return '';
    }
  }

  protected getBookmarkA11yDescription_(
      bookmark: chrome.bookmarks.BookmarkTreeNode): string {
    let description = '';
    if (this.bookmarksService?.getPriceTrackedInfo(bookmark)) {
      description += loadTimeData.getStringF(
          'a11yDescriptionPriceTracking', this.getCurrentPrice_(bookmark));
      const previousPrice = this.getPreviousPrice_(bookmark);
      if (previousPrice) {
        description += ' ' + loadTimeData.getStringF(
            'a11yDescriptionPriceChange', previousPrice);
      }
    }
    return description;
  }

  protected getBookmarkDescriptionMeta_(bookmark:
                                            chrome.bookmarks.BookmarkTreeNode) {
    // If there is a price available for the product and it isn't being
    // tracked, return the current price which will be added to the description
    // meta section.
    const productInfo =
        this.bookmarksService?.getAvailableProductInfo(bookmark);
    if (productInfo && productInfo.info.currentPrice &&
        !this.isPriceTracked_(bookmark)) {
      return productInfo.info.currentPrice;
    }

    return '';
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'power-bookmark-row': PowerBookmarkRowElement;
  }
}

customElements.define(PowerBookmarkRowElement.is, PowerBookmarkRowElement);
