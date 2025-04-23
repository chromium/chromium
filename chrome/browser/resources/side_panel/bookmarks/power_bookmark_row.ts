// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';

import type {PriceTrackingBrowserProxy} from '//resources/cr_components/commerce/price_tracking_browser_proxy.js';
import {PriceTrackingBrowserProxyImpl} from '//resources/cr_components/commerce/price_tracking_browser_proxy.js';
import type {BookmarkProductInfo} from '//resources/cr_components/commerce/shared.mojom-webui.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import type {CrUrlListItemElement} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {CrUrlListItemSize} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import type {BookmarksTreeNode} from './bookmarks.mojom-webui.js';
import {KeyArrowNavigationService} from './keyboard_arrow_navigation_service.js';
import {getCss} from './power_bookmark_row.css.js';
import {getHtml} from './power_bookmark_row.html.js';
import {PowerBookmarksService} from './power_bookmarks_service.js';
import {getFolderLabel} from './power_bookmarks_utils.js';

export const NESTED_BOOKMARKS_BASE_MARGIN = 45;
export const NESTED_BOOKMARKS_MARGIN_PER_DEPTH = 17;
export const BOOKMARK_ROW_LOAD_EVENT = 'bookmark-row-connected-event';

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
      compact: {type: Boolean},
      bookmarksTreeViewEnabled: {type: Boolean},
      contextMenuBookmark: {type: Object},
      depth: {
        type: Number,
        reflect: true,
      },
      hasCheckbox: {
        type: Boolean,
        reflect: true,
      },
      selectedBookmarks: {type: Array},
      renamingId: {type: String},
      imageUrls: {type: Object},
      isPriceTracked: {type: Boolean},
      searchQuery: {type: String},
      shoppingCollectionFolderId: {type: String},
      rowAriaDescription: {type: String},
      trailingIconTooltip: {type: String},
      listItemSize: {type: String},
      toggleExpand: {type: Boolean},
      updatedElementIds: {type: Array},
      canDrag: {type: Boolean},
    };
  }

  accessor bookmark: BookmarksTreeNode = {
    id: '',
    parentId: '',
    index: 0,
    title: '',
    url: null,
    dateAdded: null,
    dateLastUsed: null,
    unmodifiable: false,
    children: null,
  };
  accessor compact: boolean = false;
  accessor contextMenuBookmark: BookmarksTreeNode|undefined;
  accessor bookmarksTreeViewEnabled: boolean =
      loadTimeData.getBoolean('bookmarksTreeViewEnabled');
  accessor depth: number = 0;
  accessor hasCheckbox: boolean = false;
  accessor selectedBookmarks: BookmarksTreeNode[] = [];
  accessor renamingId: string = '';
  accessor searchQuery: string|undefined;
  accessor shoppingCollectionFolderId: string = '';
  accessor rowAriaDescription: string = '';
  accessor trailingIconTooltip: string = '';
  accessor toggleExpand: boolean = false;
  accessor imageUrls: {[key: string]: string} = {};
  accessor updatedElementIds: string[] = [];
  accessor isPriceTracked: boolean = false;
  accessor canDrag: boolean = true;

  accessor listItemSize: CrUrlListItemSize = CrUrlListItemSize.COMPACT;

  private bookmarksService_: PowerBookmarksService =
      PowerBookmarksService.getInstance();
  private priceTrackingProxy_: PriceTrackingBrowserProxy =
      PriceTrackingBrowserProxyImpl.getInstance();
  private shoppingListenerIds_: number[] = [];
  private keyArrowNavigationService_: KeyArrowNavigationService =
      KeyArrowNavigationService.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.onInputDisplayChange_();
    this.addEventListener('keydown', this.onKeydown_);
    this.addEventListener('focus', this.onFocus_);
    this.isPriceTracked = this.isPriceTracked_();

    const callbackRouter = this.priceTrackingProxy_.getCallbackRouter();
    this.shoppingListenerIds_.push(
        callbackRouter.priceTrackedForBookmark.addListener(
            (product: BookmarkProductInfo) =>
                this.handleBookmarkSubscriptionChange_(product, true)),
        callbackRouter.priceUntrackedForBookmark.addListener(
            (product: BookmarkProductInfo) =>
                this.handleBookmarkSubscriptionChange_(product, false)),
    );

    this.fire(BOOKMARK_ROW_LOAD_EVENT);
  }

  override disconnectedCallback() {
    this.shoppingListenerIds_.forEach(
        id => this.priceTrackingProxy_.getCallbackRouter().removeListener(id));
  }

  protected getUrl_(): string|undefined {
    return this.bookmark.url || undefined;
  }

  private handleBookmarkSubscriptionChange_(
      product: BookmarkProductInfo, subscribed: boolean): void {
    if (product.bookmarkId.toString() === this.bookmark.id) {
      this.isPriceTracked = subscribed;
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('compact')) {
      this.listItemSize =
          this.compact ? CrUrlListItemSize.COMPACT : CrUrlListItemSize.LARGE;
      if (this.bookmarksTreeViewEnabled && this.compact) {
        // Set custom margins for nested bookmarks in tree view.
        this.style.setProperty(
            '--base-margin', `${NESTED_BOOKMARKS_BASE_MARGIN}px`);
        this.style.setProperty(
            '--margin-per-depth', `${NESTED_BOOKMARKS_MARGIN_PER_DEPTH}px`);
      }
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('renamingId') ||
        changedProperties.has('bookmark')) {
      if (this.renamingId === this.bookmark?.id) {
        this.onInputDisplayChange_();
      }
    }
    if (changedProperties.has('listItemSize')) {
      this.handleListItemSizeChanged_();
    }
    if (changedProperties.has('depth')) {
      this.style.setProperty('--depth', `${this.depth}`);
    }
  }

  override shouldUpdate(changedProperties: PropertyValues<this>) {
    if (changedProperties.has('updatedElementIds')) {
      const updatedElementIds = changedProperties.get('updatedElementIds');
      if (updatedElementIds?.includes(this.bookmark?.id)) {
        return true;
      }
      changedProperties.delete('updatedElementIds');
    }
    return super.shouldUpdate(changedProperties);
  }

  override async getUpdateComplete() {
    // Wait for all children to update before marking as complete.
    const result = await super.getUpdateComplete();
    const children = [...this.shadowRoot.querySelectorAll<CrLitElement>(
        'power-bookmark-row')];
    await Promise.all(children.map(el => el.updateComplete));
    return result;
  }

  override focus() {
    this.currentUrlListItem_.focus();
  }

  private onKeydown_(e: KeyboardEvent) {
    if (this.shadowRoot.activeElement !== this.currentUrlListItem_) {
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

  getBookmarkDescriptionForTests(bookmark: BookmarksTreeNode) {
    return this.getBookmarkDescription_(bookmark);
  }

  private onFocus_(e: FocusEvent) {
    if (e.composedPath()[0] === this && this.matches(':focus-visible')) {
      // If trying to directly focus on this row, move the focus to the
      // <cr-url-list-item>. Otherwise, UI might be trying to directly focus on
      // a specific child (eg. the input).
      // This should only be done when focusing via keyboard, to avoid blocking
      // drag interactions.
      this.currentUrlListItem_.focus();
    }
  }

  get currentUrlListItem_(): CrLitElement&CrUrlListItemElement {
    return this.shadowRoot.querySelector<CrLitElement&CrUrlListItemElement>(
        '#crUrlListItem')!;
  }

  protected async handleListItemSizeChanged_() {
    await this.currentUrlListItem_.updateComplete;
    this.dispatchEvent(new CustomEvent('list-item-size-changed', {
      bubbles: true,
      composed: true,
    }));
  }

  protected isRenamingItem_(): boolean {
    return this.bookmark.id === this.renamingId;
  }

  protected isCheckboxChecked_(): boolean {
    return this.selectedBookmarks.includes(this.bookmark);
  }

  protected isBookmarksBar_(): boolean {
    return this.bookmark?.id === loadTimeData.getString('bookmarksBarId');
  }

  protected showTrailingIcon_(): boolean {
    return !this.isRenamingItem_() && !this.hasCheckbox;
  }

  protected onExpandedChanged_(event: CustomEvent<{value: boolean}>) {
    event.preventDefault();
    event.stopPropagation();
    this.toggleExpand = event.detail.value;

    // Elements are removed from the service without event emission since the
    // child elements need to be visible and present in the dom in order to be
    // seen by the parent list element and therefore remove them.
    if (!this.toggleExpand) {
      this.keyArrowNavigationService_.removeElementsWithin(this);
    }

    this.dispatchEvent(new CustomEvent('power-bookmark-toggle', {
      bubbles: true,
      composed: true,
      detail: {
        bookmark: this.bookmark,
        expanded: this.toggleExpand,
        event: event,
      },
    }));
  }

  private onInputDisplayChange_() {
    const input = this.shadowRoot.querySelector<CrInputElement>('#input');
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
    if (this.isRenamingItem_() || !this.bookmark || event.button === 2) {
      return;
    }
    // In compact view, if the item is a folder, ignore row clicks to toggle
    // the folder.
    if (this.shouldExpand_() && !this.hasCheckbox) {
      // If clicking on a row that's a folder in compact view, move focus to it.
      this.keyArrowNavigationService_.setCurrentFocusIndex(this);
      return;
    }
    event.preventDefault();
    event.stopPropagation();
    if (this.hasCheckbox && this.canEdit_()) {
      // Clicking the row should trigger a checkbox click rather than a
      // standard row click.
      const checkbox =
          this.shadowRoot.querySelector<CrCheckboxElement>('#checkbox')!;
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
        this.shadowRoot.querySelector<CrInputElement>('#input');
    if (inputElement) {
      this.dispatchEvent(this.createInputChangeEvent_(inputElement.value));
    }
  }

  protected onInputBlur_(event: Event) {
    event.preventDefault();
    event.stopPropagation();
    this.dispatchEvent(this.createInputChangeEvent_(null));
  }

  private isPriceTracked_(): boolean {
    return !!this.bookmarksService_.getPriceTrackedInfo(this.bookmark);
  }

  /**
   * Whether the given price-tracked bookmark should display as if discounted.
   */
  protected showDiscountedPrice_(): boolean {
    const bookmarkProductInfo =
        this.bookmarksService_.getPriceTrackedInfo(this.bookmark);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.previousPrice.length > 0;
    }
    return false;
  }

  protected getCurrentPrice_(bookmark: BookmarksTreeNode): string {
    const bookmarkProductInfo =
        this.bookmarksService_.getPriceTrackedInfo(bookmark);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.currentPrice;
    } else {
      return '';
    }
  }

  protected getPreviousPrice_(bookmark: BookmarksTreeNode): string {
    const bookmarkProductInfo =
        this.bookmarksService_.getPriceTrackedInfo(bookmark);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.previousPrice;
    } else {
      return '';
    }
  }

  protected getBookmarkForceHover_(): boolean {
    return this.bookmark === this.contextMenuBookmark;
  }

  protected shouldExpand_(): boolean|null {
    return this.bookmark?.children && this.bookmarksTreeViewEnabled &&
        this.compact;
  }

  protected canEdit_(): boolean {
    return this.bookmark?.id !== loadTimeData.getString('bookmarksBarId') &&
        this.bookmark?.id !==
        loadTimeData.getString('managedBookmarksFolderId');
  }

  protected isShoppingCollection_(): boolean {
    return this.bookmark?.id === this.shoppingCollectionFolderId;
  }

  protected getBookmarkDescription_(bookmark: BookmarksTreeNode): string
      |undefined {
    if (this.compact) {
      if (bookmark?.url) {
        return undefined;
      }
      const count = bookmark?.children ? bookmark?.children.length : 0;
      return loadTimeData.getStringF('bookmarkFolderChildCount', count);
    } else {
      let urlString;
      if (bookmark?.url) {
        const url = new URL(bookmark?.url);
        // Show chrome:// if it's a chrome internal url
        if (url.protocol === 'chrome:') {
          urlString = 'chrome://' + url.hostname;
        }
        urlString = url.hostname;
      }
      if (urlString && this.searchQuery && bookmark?.parentId) {
        const parentFolder =
            this.bookmarksService_.findBookmarkWithId(bookmark?.parentId);
        const folderLabel = getFolderLabel(parentFolder);
        return loadTimeData.getStringF(
            'urlFolderDescription', urlString, folderLabel);
      }
      return urlString;
    }
  }

  protected getBookmarkImageUrls_(): string[] {
    const imageUrls: string[] = [];
    if (this.bookmark?.url) {
      const imageUrl = Object.entries(this.imageUrls)
                           .find(([key, _val]) => key === this.bookmark.id)
                           ?.[1];
      if (imageUrl) {
        imageUrls.push(imageUrl);
      }
    } else if (
        this.canEdit_() && this.bookmark?.children &&
        !this.isShoppingCollection_()) {
      this.bookmark?.children.forEach(child => {
        const childImageUrl: string =
            Object.entries(this.imageUrls)
                .find(([key, _val]) => key === child.id)
                ?.[1]!;
        if (childImageUrl) {
          imageUrls.push(childImageUrl);
        }
      });
    }
    return imageUrls;
  }

  protected getBookmarkMenuA11yLabel_(): string {
    return loadTimeData.getStringF(
        this.bookmark.url ? 'bookmarkMenuLabel' : 'folderMenuLabel',
        this.bookmark.title);
  }

  protected getBookmarkA11yLabel_(): string {
    if (this.hasCheckbox) {
      if (this.isCheckboxChecked_()) {
        return loadTimeData.getStringF(
            this.bookmark.url ? 'deselectBookmarkLabel' : 'deselectFolderLabel',
            this.bookmark.title);
      }

      return loadTimeData.getStringF(
          this.bookmark.url ? 'selectBookmarkLabel' : 'selectFolderLabel',
          this.bookmark.title);
    }

    return loadTimeData.getStringF(
        this.bookmark.url ? 'openBookmarkLabel' : 'openFolderLabel',
        this.bookmark.title);
  }

  protected getBookmarkA11yDescription_(): string {
    const bookmark = this.bookmark;
    let description = '';
    if (this.bookmarksService_.getPriceTrackedInfo(bookmark)) {
      description += loadTimeData.getStringF(
          'a11yDescriptionPriceTracking', this.getCurrentPrice_(bookmark));
      const previousPrice = this.getPreviousPrice_(bookmark);
      if (previousPrice) {
        description += ' ' +
            loadTimeData.getStringF(
                'a11yDescriptionPriceChange', previousPrice);
      }
    }
    return description;
  }

  protected getBookmarkDescriptionMeta_() {
    // If there is a price available for the product and it isn't being
    // tracked, return the current price which will be added to the description
    // meta section.
    const productInfo =
        this.bookmarksService_.getAvailableProductInfo(this.bookmark);
    if (productInfo && productInfo.info.currentPrice &&
        !this.isPriceTracked_()) {
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
