// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//bookmarks-side-panel.top-chrome/shared/sp_list_item_badge.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import './icons.html.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import type {CrUrlListItemElement} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {CrUrlListItemSize} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import type {BookmarksTreeNode} from './bookmarks.mojom-webui.js';
import {getCss} from './power_bookmark_row_item.css.js';
import {getHtml} from './power_bookmark_row_item.html.js';
import {PowerBookmarksService} from './power_bookmarks_service.js';
import {getFolderLabel} from './power_bookmarks_utils.js';

export interface PowerBookmarkRowItemElement {
  $: {
    crUrlListItem: CrUrlListItemElement,
  };
}

export class PowerBookmarkRowItemElement extends CrLitElement {
  static get is() {
    return 'power-bookmark-row-item';
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
      contextMenuBookmark: {type: Object},
      depth: {
        type: Number,
        reflect: true,
      },
      hasCheckbox: {
        type: Boolean,
        reflect: true,
      },
      hasFolders: {
        type: Boolean,
        reflect: true,
      },
      imageUrls: {type: Object},
      isPriceTracked: {type: Boolean},
      searchQuery: {type: String},
      shoppingCollectionFolderId: {type: String},
      trailingIconTooltip: {type: String},
      listItemSize: {type: String},
      isSelected: {type: Boolean},
      selectedBookmarks: {type: Array},
      renamingId: {type: String},
      hasActiveDrag: {type: Boolean},
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
  accessor depth: number = 0;
  accessor hasCheckbox: boolean = false;
  accessor hasFolders: boolean = false;
  accessor imageUrls: {[key: string]: string} = {};
  accessor isPriceTracked: boolean = false;
  accessor searchQuery: string|undefined;
  accessor shoppingCollectionFolderId: string = '';
  accessor trailingIconTooltip: string = '';
  accessor listItemSize: CrUrlListItemSize = CrUrlListItemSize.COMPACT;
  accessor isSelected: boolean = false;
  accessor selectedBookmarks: BookmarksTreeNode[] = [];
  accessor renamingId: string = '';
  accessor hasActiveDrag: boolean = false;

  private bookmarksService_: PowerBookmarksService =
      PowerBookmarksService.getInstance();

  override focus() {
    this.$.crUrlListItem.focus();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('renamingId') ||
        changedProperties.has('bookmark')) {
      if (this.renamingId === this.bookmark?.id) {
        this.onInputDisplayChange_();
      }
    }
  }

  private onInputDisplayChange_() {
    const input = this.shadowRoot.querySelector<CrInputElement>('#input');
    if (input) {
      input.select();
    }
  }

  protected getUrl_(): string|undefined {
    return this.bookmark.url || undefined;
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

  protected onClick_(event: MouseEvent) {
    this.onRowClicked_(event);
  }

  protected onAuxclick_(event: MouseEvent) {
    this.onRowClicked_(event);
  }

  protected onRowClicked_(event: MouseEvent) {
    if (this.isRenamingItem_() || !this.bookmark || event.button === 2 ||
        this.hasActiveDrag) {
      return;
    }
    event.preventDefault();
    event.stopPropagation();
    if (this.hasCheckbox && this.canEdit_()) {
      const checkbox =
          this.shadowRoot.querySelector<CrCheckboxElement>('#checkbox')!;
      checkbox.checked = !checkbox.checked;
      return;
    }
    this.fire('row-clicked', {bookmark: this.bookmark, event: event});
  }

  protected onContextmenu_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.fire('context-menu', {bookmark: this.bookmark, event: event});
  }

  protected onTrailingIconClick_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.fire('trailing-icon-clicked', {bookmark: this.bookmark, event: event});
  }

  protected onCheckboxCheckedChanged_(event: Event) {
    event.preventDefault();
    event.stopPropagation();
    this.fire('checkbox-change', {
      bookmark: this.bookmark,
      checked: (event.target as CrCheckboxElement).checked,
    });
  }

  protected onInputKeydown_(event: KeyboardEvent) {
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

  getBookmarkDescriptionForTests(bookmark: BookmarksTreeNode) {
    return this.getBookmarkDescription_(bookmark);
  }

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
    'power-bookmark-row-item': PowerBookmarkRowItemElement;
  }
}

customElements.define(
    PowerBookmarkRowItemElement.is, PowerBookmarkRowItemElement);
