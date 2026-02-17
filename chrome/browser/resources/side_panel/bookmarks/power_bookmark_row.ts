// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import './power_bookmark_row_item.js';

import type {PriceTrackingBrowserProxy} from '//resources/cr_components/commerce/price_tracking_browser_proxy.js';
import {PriceTrackingBrowserProxyImpl} from '//resources/cr_components/commerce/price_tracking_browser_proxy.js';
import type {BookmarkProductInfo} from '//resources/cr_components/commerce/shared.mojom-webui.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrUrlListItemSize} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isRTL} from 'chrome://resources/js/util.js';

import type {BookmarksTreeNode} from './bookmarks.mojom-webui.js';
import {KeyArrowNavigationService} from './keyboard_arrow_navigation_service.js';
import {getCss} from './power_bookmark_row.css.js';
import {getHtml} from './power_bookmark_row.html.js';
import type {PowerBookmarkRowItemElement} from './power_bookmark_row_item.js';
import {PowerBookmarksService} from './power_bookmarks_service.js';

export const NESTED_BOOKMARKS_BASE_MARGIN = 28;
export const NESTED_BOOKMARKS_MARGIN_PER_DEPTH = 12;
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
      isSelected: {type: Boolean},
      updatedElementIds: {type: Array},
      canDrag: {type: Boolean},
      hasActiveDrag: {type: Boolean},
      activeFolderPath: {type: Array},
      hasFolders: {type: Boolean, reflect: true},
      sortedChildren: {type: Array},
      activeSortIndex: {type: Number},
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
  accessor isSelected: boolean = false;
  accessor imageUrls: {[key: string]: string} = {};
  accessor updatedElementIds: string[] = [];
  accessor isPriceTracked: boolean = false;
  accessor canDrag: boolean = true;
  accessor hasActiveDrag: boolean = false;
  accessor activeFolderPath: BookmarksTreeNode[] = [];
  accessor hasFolders: boolean = false;
  accessor sortedChildren: BookmarksTreeNode[] = [];
  accessor activeSortIndex: number = 0;

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
    super.disconnectedCallback();
    this.shoppingListenerIds_.forEach(
        id => this.priceTrackingProxy_.getCallbackRouter().removeListener(id));
  }

  private handleBookmarkSubscriptionChange_(
      product: BookmarkProductInfo, subscribed: boolean): void {
    if (product.bookmarkId.toString() === this.bookmark.id) {
      this.isPriceTracked = subscribed;
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('bookmark') &&
        this.bookmark.id !== changedProperties.get('bookmark')?.id) {
      this.toggleExpand = false;
      this.sortedChildren =
          this.bookmark.children ? [...this.bookmark.children] : [];
      this.bookmarksService_.sortBookmarks(
          this.sortedChildren, this.activeSortIndex);
    }

    if (changedProperties.has('activeFolderPath')) {
      this.isSelected = this.activeFolderPath?.length > 0 &&
          this.activeFolderPath[this.activeFolderPath.length - 1].id ===
              this.bookmark.id;
    }

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

    if (changedProperties.has('activeSortIndex')) {
      this.bookmarksService_.sortBookmarks(
          this.sortedChildren, this.activeSortIndex);
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

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
    this.currentListItem_.focus();
  }

  private setExpanded_(expanded: boolean, event?: Event) {
    if (!this.isFolder_() || this.toggleExpand === expanded) {
      return;
    }
    this.toggleExpand = expanded;

    if (!this.toggleExpand) {
      this.keyArrowNavigationService_.removeElementsWithin(this);
    }

    this.fire('power-bookmark-toggle', {
      bookmark: this.bookmark,
      expanded: this.toggleExpand,
      event: event,
    });
  }

  private onKeydown_(e: KeyboardEvent) {
    if (this.shadowRoot.activeElement !== this.currentListItem_) {
      return;
    }

    const isRtl = isRTL();
    const forwardKey = isRtl ? 'ArrowLeft' : 'ArrowRight';
    const backwardKey = isRtl ? 'ArrowRight' : 'ArrowLeft';

    if (e.key === forwardKey) {
      if (this.isFolder_()) {
        if (!this.toggleExpand) {
          this.setExpanded_(true);
        } else if (this.isFolderWithChildren_()) {
          this.keyArrowNavigationService_.moveFocus(1);
        }
      }
      e.stopPropagation();
      return;
    }

    if (e.key === backwardKey) {
      if (this.isFolder_() && this.toggleExpand) {
        this.setExpanded_(false);
      } else {
        const parentRow =
            (this.getRootNode() as ShadowRoot)?.host as HTMLElement;
        parentRow.focus();
        this.keyArrowNavigationService_.setCurrentFocusIndex(parentRow);
      }
      e.stopPropagation();
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
    return this.currentListItem_.getBookmarkDescriptionForTests(bookmark);
  }

  private onFocus_(e: FocusEvent) {
    if (e.composedPath()[0] === this && this.matches(':focus-visible')) {
      // If trying to directly focus on this row, move the focus to the
      // <cr-url-list-item>. Otherwise, UI might be trying to directly focus on
      // a specific child (eg. the input).
      // This should only be done when focusing via keyboard, to avoid blocking
      // drag interactions.
      this.currentListItem_.focus();
    }
  }

  get currentListItem_(): PowerBookmarkRowItemElement {
    return this.shadowRoot.querySelector<PowerBookmarkRowItemElement>(
        '#listItem')!;
  }

  protected async handleListItemSizeChanged_() {
    await this.currentListItem_.updateComplete;
    this.fire('list-item-size-changed');
  }

  protected onExpandedChanged_(event: CustomEvent<{value: boolean}>) {
    event.preventDefault();
    event.stopPropagation();
    this.setExpanded_(event.detail.value, event);
  }

  private isPriceTracked_(): boolean {
    return !!this.bookmarksService_.getPriceTrackedInfo(this.bookmark);
  }

  protected shouldExpand_(): boolean|null {
    return this.bookmark?.children && this.bookmarksTreeViewEnabled &&
        this.compact;
  }

  protected isFolder_(): boolean {
    return !this.bookmark.url;
  }

  protected isFolderWithChildren_(): boolean {
    return this.isFolder_() && !!this.bookmark.children?.length;
  }

  protected getWrapperId_(): string {
    return this.compact && this.bookmarksTreeViewEnabled ? 'bookmark' : '';
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'power-bookmark-row': PowerBookmarkRowElement;
  }
}

customElements.define(PowerBookmarkRowElement.is, PowerBookmarkRowElement);
