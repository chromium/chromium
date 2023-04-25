// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import './commerce/shopping_list.js';
import './icons.html.js';
import './power_bookmarks_context_menu.js';
import './power_bookmark_row.js';
import './power_bookmarks_context_menu.js';
import './power_bookmarks_edit_dialog.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_empty_state.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_filter_chip.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_footer.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_heading.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_icons.html.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_list_item_badge.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_shared_style.css.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '//resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import '//resources/cr_elements/cr_toolbar/cr_toolbar_selection_overlay.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';

import {startColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrLazyRenderElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {CrToolbarSearchFieldElement} from '//resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import {listenOnce} from '//resources/js/util_ts.js';
import {IronListElement} from '//resources/polymer/v3_0/iron-list/iron-list.js';
import {DomRepeatEvent, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ActionSource, SortOrder, ViewType} from './bookmarks.mojom-webui.js';
import {BookmarksApiProxy, BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';
import {ShoppingListApiProxy, ShoppingListApiProxyImpl} from './commerce/shopping_list_api_proxy.js';
import {PowerBookmarksContextMenuElement} from './power_bookmarks_context_menu.js';
import {PowerBookmarksEditDialogElement} from './power_bookmarks_edit_dialog.js';
import {getTemplate} from './power_bookmarks_list.html.js';
import {editingDisabledByPolicy, Label, PowerBookmarksService} from './power_bookmarks_service.js';
import {BookmarkProductInfo} from './shopping_list.mojom-webui.js';

const ADD_FOLDER_ACTION_UMA = 'Bookmarks.FolderAddedFromSidePanel';
const ADD_URL_ACTION_UMA = 'Bookmarks.AddedFromSidePanel';

function getBookmarkName(bookmark: chrome.bookmarks.BookmarkTreeNode): string {
  return bookmark.title || bookmark.url || '';
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This must be kept in sync with
// BookmarksSidePanelSearchCTREvent in tools/metrics/histograms/enums.xml.
export enum SearchAction {
  SHOWN = 0,
  SEARCHED = 1,

  // Must be last.
  COUNT = 2,
}

export interface SortOption {
  sortOrder: SortOrder;
  label: string;
  lowerLabel: string;
}

export interface PowerBookmarksListElement {
  $: {
    bookmarks: HTMLElement,
    contextMenu: PowerBookmarksContextMenuElement,
    deletionToast: CrLazyRenderElement<CrToastElement>,
    powerBookmarksContainer: HTMLElement,
    searchField: CrToolbarSearchFieldElement,
    shownBookmarksIronList: IronListElement,
    sortMenu: CrActionMenuElement,
    editDialog: PowerBookmarksEditDialogElement,
    disabledFeatureDialog: CrDialogElement,
  };
}

export class PowerBookmarksListElement extends PolymerElement {
  static get is() {
    return 'power-bookmarks-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      shownBookmarks_: {
        type: Array,
        value: () => [],
      },

      compact_: {
        type: Boolean,
        value: () => loadTimeData.getInteger('viewType') === 0,
      },

      activeFolderPath_: {
        type: Array,
        value: () => [],
      },

      labels_: {
        type: Array,
        value: () => [],
        computed: 'computePriceTrackingLabel_(trackedProductInfos_.*)',
      },

      activeSortIndex_: {
        type: Number,
        value: () => loadTimeData.getInteger('sortOrder'),
      },

      sortTypes_: {
        type: Array,
        value: () =>
            [{
              sortOrder: SortOrder.kNewest,
              label: loadTimeData.getString('sortNewest'),
              lowerLabel: loadTimeData.getString('sortNewestLower'),
            },
             {
               sortOrder: SortOrder.kOldest,
               label: loadTimeData.getString('sortOldest'),
               lowerLabel: loadTimeData.getString('sortOldestLower'),
             },
             {
               sortOrder: SortOrder.kLastOpened,
               label: loadTimeData.getString('sortLastOpened'),
               lowerLabel: loadTimeData.getString('sortLastOpenedLower'),
             },
             {
               sortOrder: SortOrder.kAlphabetical,
               label: loadTimeData.getString('sortAlphabetically'),
               lowerLabel: loadTimeData.getString('sortAlphabetically'),
             },
             {
               sortOrder: SortOrder.kReverseAlphabetical,
               label: loadTimeData.getString('sortReverseAlphabetically'),
               lowerLabel: loadTimeData.getString('sortReverseAlphabetically'),
             }],
      },

      editing_: {
        type: Boolean,
        value: false,
      },

      selectedBookmarks_: {
        type: Array,
        value: () => [],
      },

      guestMode_: {
        type: Boolean,
        value: loadTimeData.getBoolean('guestMode'),
        reflectToAttribute: true,
      },

      renamingId_: {
        type: String,
        value: '',
      },

      deletionDescription_: {
        type: String,
        value: '',
      },

      /* If container containing shown bookmarks has scrollbars. */
      hasScrollbars_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  static get observers() {
    return [
      'updateShownBookmarks_(activeFolderPath_.*, labels_.*, ' +
          'activeSortIndex_, searchQuery_)',
    ];
  }

  private bookmarksApi_: BookmarksApiProxy =
      BookmarksApiProxyImpl.getInstance();
  private shoppingListApi_: ShoppingListApiProxy =
      ShoppingListApiProxyImpl.getInstance();
  private shoppingListenerIds_: number[] = [];
  private shownBookmarks_: chrome.bookmarks.BookmarkTreeNode[];
  private trackedProductInfos_ = new Map<string, BookmarkProductInfo>();
  private availableProductInfos_ = new Map<string, BookmarkProductInfo>();
  private bookmarksService_: PowerBookmarksService =
      new PowerBookmarksService(this);
  private compact_: boolean;
  private activeFolderPath_: chrome.bookmarks.BookmarkTreeNode[];
  private labels_: Label[];
  private compactDescriptions_ = new Map<string, string>();
  private expandedDescriptions_ = new Map<string, string>();
  private imageUrls_ = new Map<string, string>();
  private activeSortIndex_: number;
  private sortTypes_: SortOption[];
  private searchQuery_: string|undefined;
  private currentUrl_: string|undefined;
  private editing_: boolean;
  private selectedBookmarks_: chrome.bookmarks.BookmarkTreeNode[];
  private guestMode_: boolean;
  private renamingId_: string;
  private deletionDescription_: string;
  private shownBookmarksResizeObserver_?: ResizeObserver;
  private hasScrollbars_: boolean;
  private contextMenuBookmark_: chrome.bookmarks.BookmarkTreeNode|undefined;

  constructor() {
    super();
    startColorChangeUpdater();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setAttribute('role', 'tree');
    listenOnce(this.$.powerBookmarksContainer, 'dom-change', () => {
      setTimeout(() => this.bookmarksApi_.showUi(), 0);
    });
    this.bookmarksService_.startListening();
    this.shoppingListApi_.getAllPriceTrackedBookmarkProductInfo().then(res => {
      res.productInfos.forEach(
          product => this.set(
              `trackedProductInfos_.${product.bookmarkId.toString()}`,
              product));
    });
    this.shoppingListApi_.getAllShoppingBookmarkProductInfo().then(res => {
      res.productInfos.forEach(
          product => this.setAvailableProductInfo_(product));
    });
    const callbackRouter = this.shoppingListApi_.getCallbackRouter();
    this.shoppingListenerIds_.push(
        callbackRouter.priceTrackedForBookmark.addListener(
            (product: BookmarkProductInfo) =>
                this.onBookmarkPriceTracked_(product)),
        callbackRouter.priceUntrackedForBookmark.addListener(
            (bookmarkId: bigint) =>
                this.onBookmarkPriceUntracked_(bookmarkId.toString())),
    );

    if (document.documentElement.hasAttribute('chrome-refresh-2023')) {
      this.shownBookmarksResizeObserver_ =
          new ResizeObserver(this.onShownBookmarksResize_.bind(this));
      this.shownBookmarksResizeObserver_.observe(this.$.bookmarks);
    }

    this.recordMetricsOnConnected_();
  }

  override disconnectedCallback() {
    this.bookmarksService_.stopListening();
    this.shoppingListenerIds_.forEach(
        id => this.shoppingListApi_.getCallbackRouter().removeListener(id));

    if (this.shownBookmarksResizeObserver_) {
      this.shownBookmarksResizeObserver_.disconnect();
      this.shownBookmarksResizeObserver_ = undefined;
    }
  }

  setCurrentUrl(url: string) {
    this.currentUrl_ = url;
  }

  setCompactDescription(
      bookmark: chrome.bookmarks.BookmarkTreeNode, description: string) {
    this.set(`compactDescriptions_.${bookmark.id}`, description);
  }

  setExpandedDescription(
      bookmark: chrome.bookmarks.BookmarkTreeNode, description: string) {
    this.set(`expandedDescriptions_.${bookmark.id}`, description);
  }

  setImageUrl(bookmark: chrome.bookmarks.BookmarkTreeNode, url: string) {
    this.set(`imageUrls_.${bookmark.id.toString()}`, url);
  }

  onBookmarksLoaded() {
    this.updateShownBookmarks_();
  }

  onBookmarkChanged(id: string, changedInfo: chrome.bookmarks.ChangeInfo) {
    Object.keys(changedInfo).forEach(key => {
      const visibleIndex = this.visibleIndex_(id);
      if (visibleIndex > -1) {
        this.notifyPath(`shownBookmarks_.${visibleIndex}.${key}`);
      }
    });
    this.updateShoppingData_();
  }

  onBookmarkCreated(
      bookmark: chrome.bookmarks.BookmarkTreeNode,
      parent: chrome.bookmarks.BookmarkTreeNode) {
    if (this.visibleParent_(parent)) {
      this.shownBookmarks_.unshift(bookmark);
      this.bookmarksService_.sortBookmarks(
          this.shownBookmarks_, this.activeSortIndex_);
      this.shownBookmarks_ = this.shownBookmarks_.slice();
      const bookmarkIndex = this.shownBookmarks_.indexOf(bookmark);
      this.$.shownBookmarksIronList.scrollToIndex(bookmarkIndex);
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkCreated', getBookmarkName(bookmark)));
    }
    this.updateShoppingData_();
  }

  onBookmarkMoved(
      bookmark: chrome.bookmarks.BookmarkTreeNode,
      oldParent: chrome.bookmarks.BookmarkTreeNode,
      newParent: chrome.bookmarks.BookmarkTreeNode) {
    const shouldUpdateUIAdded = this.visibleParent_(newParent);
    const shouldUpdateUIRemoved = this.visibleParent_(oldParent);
    const shouldUpdateUIReordered =
        shouldUpdateUIAdded && shouldUpdateUIRemoved;

    if (shouldUpdateUIReordered) {
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkReordered', getBookmarkName(bookmark)));
    } else if (shouldUpdateUIAdded) {
      this.shownBookmarks_.unshift(bookmark);
      this.bookmarksService_.sortBookmarks(
          this.shownBookmarks_, this.activeSortIndex_);
      this.shownBookmarks_ = this.shownBookmarks_.slice();
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkMoved', getBookmarkName(bookmark),
          getBookmarkName(newParent)));
    } else if (shouldUpdateUIRemoved) {
      this.splice('shownBookmarks_', this.visibleIndex_(bookmark.id), 1);
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkMoved', getBookmarkName(bookmark),
          getBookmarkName(newParent)));
    }
  }

  onBookmarkRemoved(bookmark: chrome.bookmarks.BookmarkTreeNode) {
    const visibleIndex = this.visibleIndex_(bookmark.id);
    if (visibleIndex > -1) {
      this.splice('shownBookmarks_', visibleIndex, 1);
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkDeleted', getBookmarkName(bookmark)));
    }
    this.set(`trackedProductInfos_.${bookmark.id}`, null);
    this.availableProductInfos_.delete(bookmark.id);
  }

  isPriceTracked(bookmark: chrome.bookmarks.BookmarkTreeNode): boolean {
    return !!this.get(`trackedProductInfos_.${bookmark.id}`);
  }

  getProductImageUrl(bookmark: chrome.bookmarks.BookmarkTreeNode): string {
    const bookmarkProductInfo = this.availableProductInfos_.get(bookmark.id);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.imageUrl.url;
    } else {
      return '';
    }
  }

  private isPriceTrackingEligible_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      boolean {
    return !!this.availableProductInfos_.get(bookmark.id);
  }

  private onBookmarkPriceTracked_(product: BookmarkProductInfo) {
    this.set(`trackedProductInfos_.${product.bookmarkId.toString()}`, product);
  }

  private onBookmarkPriceUntracked_(bookmarkId: string) {
    this.set(`trackedProductInfos_.${bookmarkId}`, null);
  }

  // TODO(emshack): Once there is more than one bookmark power, remove this
  // logic and always display the price tracking label button.
  private computePriceTrackingLabel_() {
    const showLabel =
        Object.keys(this.trackedProductInfos_)
            .some(key => this.get(`trackedProductInfos_.${key}`) !== null);
    if (showLabel) {
      return [{
        label: loadTimeData.getString('priceTrackingLabel'),
        icon: 'bookmarks:price-tracking',
        active: false,
      }];
    } else {
      return [];
    }
  }

  /**
   * Returns the index of the given node id in the currently shown bookmarks,
   * or -1 if not shown.
   */
  private visibleIndex_(nodeId: string): number {
    return this.shownBookmarks_.findIndex(b => b.id === nodeId);
  }

  /**
   * Returns true if the given node is either the current active folder or a
   * root folder that isn't shown itself while the all bookmarks list is shown.
   */
  private visibleParent_(parent: chrome.bookmarks.BookmarkTreeNode): boolean {
    const activeFolder = this.getActiveFolder_();
    return (!activeFolder && parent.parentId === '0' &&
            this.visibleIndex_(parent.id) === -1) ||
        parent === activeFolder;
  }

  private getActiveFolder_(): chrome.bookmarks.BookmarkTreeNode|undefined {
    if (this.activeFolderPath_.length) {
      return this.activeFolderPath_[this.activeFolderPath_.length - 1];
    }
    return undefined;
  }

  private getBackButtonLabel_(): string {
    const activeFolder = this.getActiveFolder_();
    const parentFolder = this.bookmarksService_.findBookmarkWithId(
        activeFolder ? activeFolder.parentId : undefined);
    return loadTimeData.getStringF(
        'backButtonLabel', this.getFolderLabel_(parentFolder));
  }

  private getBookmarksListRole_(): string {
    return this.editing_ ? 'listbox' : 'list';
  }

  private getBookmarkDescription_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      string|undefined {
    if (this.compact_) {
      return this.get(`compactDescriptions_.${bookmark.id}`);
    } else {
      const url = this.get(`expandedDescriptions_.${bookmark.id}`);
      if (this.searchQuery_ && url && bookmark.parentId) {
        const parentFolder =
            this.bookmarksService_.findBookmarkWithId(bookmark.parentId);
        const folderLabel = this.getFolderLabel_(parentFolder);
        return loadTimeData.getStringF(
            'urlFolderDescription', url, folderLabel);
      } else {
        return url;
      }
    }
  }

  private getBookmarkAllyLabel_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      string {
    if (bookmark.url) {
      return loadTimeData.getStringF('openBookmarkLabel', bookmark.title);
    } else {
      return loadTimeData.getStringF('openFolderLabel', bookmark.title);
    }
  }

  private getBookmarkA11yDescription_(
      bookmark: chrome.bookmarks.BookmarkTreeNode): string {
    let description = '';
    if (this.isPriceTracked(bookmark)) {
      description += loadTimeData.getStringF(
          'a11yDescriptionPriceTracking', this.getCurrentPrice_(bookmark));
      const previousPrice = this.getPreviousPrice_(bookmark);
      if (previousPrice) {
        description += loadTimeData.getStringF(
            'a11yDescriptionPriceChange', previousPrice);
      }
    }
    return description;
  }

  private getBookmarkImageUrls_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      string[] {
    const imageUrls: string[] = [];
    if (bookmark.url) {
      const imageUrl = this.get(`imageUrls_.${bookmark.id.toString()}`);
      if (imageUrl) {
        imageUrls.push(imageUrl);
      }
    } else if (this.canEdit_(bookmark) && bookmark.children) {
      bookmark.children.forEach((child) => {
        const childImageUrl: string =
            this.get(`imageUrls_.${child.id.toString()}`);
        if (childImageUrl) {
          imageUrls.push(childImageUrl);
        }
      });
    }
    return imageUrls;
  }

  private getBookmarkForceHover_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      boolean {
    return bookmark === this.contextMenuBookmark_;
  }

  private getActiveFolderLabel_(): string {
    return this.getFolderLabel_(this.getActiveFolder_());
  }

  private getFolderLabel_(folder: chrome.bookmarks.BookmarkTreeNode|
                          undefined): string {
    if (folder && folder.id !== loadTimeData.getString('otherBookmarksId') &&
        folder.id !== loadTimeData.getString('mobileBookmarksId')) {
      return folder!.title;
    } else {
      return loadTimeData.getString('allBookmarks');
    }
  }

  private getSortLabel_(): string {
    return this.sortTypes_[this.activeSortIndex_]!.label;
  }

  private renamingItem_(id: string) {
    return id === this.renamingId_;
  }

  private updateShoppingData_() {
    this.availableProductInfos_.clear();
    this.shoppingListApi_.getAllShoppingBookmarkProductInfo().then(res => {
      res.productInfos.forEach(
          product => this.setAvailableProductInfo_(product));
    });
  }

  private setAvailableProductInfo_(productInfo: BookmarkProductInfo) {
    const bookmarkId = productInfo.bookmarkId.toString();
    this.availableProductInfos_.set(bookmarkId, productInfo);
    if (productInfo.info.imageUrl.url !== '') {
      const bookmark = this.bookmarksService_.findBookmarkWithId(bookmarkId)!;
      this.setImageUrl(bookmark, productInfo.info.imageUrl.url);
    }
  }

  /**
   * Update the list of bookmarks and folders displayed to the user.
   */
  private updateShownBookmarks_() {
    this.shownBookmarks_ = this.bookmarksService_.filterBookmarks(
        this.getActiveFolder_(), this.activeSortIndex_, this.searchQuery_,
        this.labels_);
    this.bookmarksService_.refreshDataForBookmarks(this.shownBookmarks_);
  }

  private recordMetricsOnConnected_() {
    chrome.metricsPrivate.recordEnumerationValue(
        'PowerBookmarks.SidePanel.SortTypeShown',
        this.sortTypes_[this.activeSortIndex_].sortOrder, SortOrder.kCount);
    chrome.metricsPrivate.recordEnumerationValue(
        'PowerBookmarks.SidePanel.ViewTypeShown',
        this.compact_ ? ViewType.kCompact : ViewType.kExpanded,
        ViewType.kCount);
    chrome.metricsPrivate.recordEnumerationValue(
        'PowerBookmarks.SidePanel.Search.CTR', SearchAction.SHOWN,
        SearchAction.COUNT);
  }

  private canAddCurrentUrl_(): boolean {
    return this.bookmarksService_.canAddUrl(
        this.currentUrl_, this.getActiveFolder_());
  }

  private canEdit_(bookmark: chrome.bookmarks.BookmarkTreeNode): boolean {
    return bookmark.id !== loadTimeData.getString('bookmarksBarId') &&
        bookmark.id !== loadTimeData.getString('managedBookmarksFolderId');
  }

  private getSortMenuItemLabel_(sortType: SortOption): string {
    return loadTimeData.getStringF('sortByType', sortType.label);
  }

  private getSortMenuItemLowerLabel_(sortType: SortOption): string {
    return loadTimeData.getStringF('sortByType', sortType.lowerLabel);
  }

  private sortMenuItemIsSelected_(sortType: SortOption): boolean {
    return this.sortTypes_[this.activeSortIndex_].sortOrder ===
        sortType.sortOrder;
  }

  /**
   * Invoked when the user clicks a power bookmarks row. This will either
   * display children in the case of a folder row, or open the URL in the case
   * of a bookmark row.
   */
  private onRowClicked_(
      event: CustomEvent<
          {bookmark: chrome.bookmarks.BookmarkTreeNode, event: MouseEvent}>) {
    event.preventDefault();
    event.stopPropagation();
    if (!this.editing_) {
      if (event.detail.bookmark.children) {
        this.push('activeFolderPath_', event.detail.bookmark);
        // Cancel search when changing active folder.
        this.$.searchField.setValue('');
      } else {
        this.bookmarksApi_.openBookmark(
            event.detail.bookmark.id, this.activeFolderPath_.length, {
              middleButton: false,
              altKey: event.detail.event.altKey,
              ctrlKey: event.detail.event.ctrlKey,
              metaKey: event.detail.event.metaKey,
              shiftKey: event.detail.event.shiftKey,
            },
            ActionSource.kBookmark);
      }
    }
  }

  private onRowSelectedChange_(
      event: CustomEvent<
          {bookmark: chrome.bookmarks.BookmarkTreeNode, checked: boolean}>) {
    event.preventDefault();
    event.stopPropagation();
    if (event.detail.checked) {
      this.unshift('selectedBookmarks_', event.detail.bookmark);
    } else {
      this.splice(
          'selectedBookmarks_',
          this.selectedBookmarks_.findIndex(b => b === event.detail.bookmark),
          1);
    }
  }

  private async onBookmarksEdited_(event: CustomEvent<{
    bookmarks: chrome.bookmarks.BookmarkTreeNode[],
    name: string|undefined,
    url: string|undefined,
    folderId: string,
    newFolders: chrome.bookmarks.BookmarkTreeNode[],
  }>) {
    event.preventDefault();
    event.stopPropagation();
    let parentId = event.detail.folderId;
    for (const folder of event.detail.newFolders) {
      chrome.metricsPrivate.recordUserAction(ADD_FOLDER_ACTION_UMA);
      const newFolder =
          await this.bookmarksApi_.createFolder(folder.parentId!, folder.title);
      folder.children!.forEach(child => child.parentId = newFolder.id);
      if (folder.id === parentId) {
        parentId = newFolder.id;
      }
    }
    this.bookmarksApi_.editBookmarks(
        event.detail.bookmarks.map(bookmark => bookmark.id), event.detail.name,
        event.detail.url, parentId);
    this.selectedBookmarks_ = [];
    this.editing_ = false;
  }

  private setRenamingId_(event: CustomEvent<{id: string}>) {
    this.renamingId_ = event.detail.id;
  }

  private onRename_(
      event: CustomEvent<
          {bookmark: chrome.bookmarks.BookmarkTreeNode, value: string}>) {
    this.bookmarksApi_.renameBookmark(
        event.detail.bookmark.id, event.detail.value);
    this.renamingId_ = '';
  }

  private hasActiveLabels_(): boolean {
    for (const label of this.labels_) {
      if (label.active) {
        return true;
      }
    }
    return false;
  }

  private shouldShowEmptySearchState_() {
    return this.hasActiveLabels_() || !!this.searchQuery_;
  }

  private shouldHideHeader_(): boolean {
    return this.hasActiveLabels_() || !!this.searchQuery_;
  }

  private getSelectedDescription_() {
    return loadTimeData.getStringF(
        'selectedBookmarkCount', this.selectedBookmarks_.length);
  }

  /**
   * Returns the appropriate filter button icon depending on whether the given
   * label is active.
   */
  private getLabelIcon_(label: Label): string {
    if (label.active) {
      return 'bookmarks:check';
    } else {
      return label.icon;
    }
  }

  /**
   * Toggles the given label between active and inactive.
   */
  private onLabelClicked_(event: DomRepeatEvent<Label>) {
    event.preventDefault();
    event.stopPropagation();
    const label = event.model.item;
    this.set(`labels_.${event.model.index}.active`, !label.active);
  }

  /**
   * Moves the displayed folders up one level when the back button is clicked.
   */
  private onBackClicked_() {
    this.pop('activeFolderPath_');
  }

  private onSearchChanged_(e: CustomEvent<string>) {
    this.searchQuery_ = e.detail.toLocaleLowerCase();
  }

  private onSearchBlurred_() {
    chrome.metricsPrivate.recordEnumerationValue(
        'PowerBookmarks.SidePanel.Search.CTR', SearchAction.SEARCHED,
        SearchAction.COUNT);
  }

  private onShowContextMenuClicked_(
      event: CustomEvent<
          {bookmark: chrome.bookmarks.BookmarkTreeNode, event: MouseEvent}>) {
    event.preventDefault();
    event.stopPropagation();
    const priceTracked = this.isPriceTracked(event.detail.bookmark);
    const priceTrackingEligible =
        this.isPriceTrackingEligible_(event.detail.bookmark);
    this.contextMenuBookmark_ = event.detail.bookmark;
    if (event.detail.event.button === 0) {
      this.$.contextMenu.showAt(
          event.detail.event, [this.contextMenuBookmark_], priceTracked,
          priceTrackingEligible);
    } else {
      this.$.contextMenu.showAtPosition(
          event.detail.event, [this.contextMenuBookmark_], priceTracked,
          priceTrackingEligible);
    }
  }

  private getParentFolder_(): chrome.bookmarks.BookmarkTreeNode {
    return this.getActiveFolder_() ||
        this.bookmarksService_.findBookmarkWithId(
            loadTimeData.getString('otherBookmarksId'))!;
  }

  private onShowSortMenuClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.$.sortMenu.showAt(event.target as HTMLElement);
  }

  private onAddNewFolderClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    const newParent = this.getParentFolder_();
    if (editingDisabledByPolicy([newParent])) {
      this.showDisabledFeatureDialog_();
      return;
    }
    chrome.metricsPrivate.recordUserAction(ADD_FOLDER_ACTION_UMA);
    this.bookmarksApi_
        .createFolder(newParent.id, loadTimeData.getString('newFolderTitle'))
        .then((newFolder) => {
          this.renamingId_ = newFolder.id;
        });
  }

  private onBulkEditClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.editing_ = !this.editing_;
    if (!this.editing_) {
      this.selectedBookmarks_ = [];
    }
  }

  private onDeleteClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    if (editingDisabledByPolicy(this.selectedBookmarks_)) {
      this.showDisabledFeatureDialog_();
      return;
    }
    this.bookmarksApi_
        .deleteBookmarks(this.selectedBookmarks_.map(bookmark => bookmark.id))
        .then(() => {
          this.showDeletionToastWithCount_(this.selectedBookmarks_.length);
          this.selectedBookmarks_ = [];
          this.editing_ = false;
        });
  }

  private onContextMenuEditClicked_(
      event: CustomEvent<{bookmarks: chrome.bookmarks.BookmarkTreeNode[]}>) {
    event.preventDefault();
    event.stopPropagation();
    if (editingDisabledByPolicy(event.detail.bookmarks)) {
      this.showDisabledFeatureDialog_();
      return;
    }
    this.showEditDialog_(
        event.detail.bookmarks, event.detail.bookmarks.length > 1);
  }

  private onContextMenuDeleteClicked_(
      event: CustomEvent<{bookmarks: chrome.bookmarks.BookmarkTreeNode[]}>) {
    event.preventDefault();
    event.stopPropagation();
    this.showDeletionToastWithCount_(event.detail.bookmarks.length);
    this.selectedBookmarks_ = [];
    this.editing_ = false;
  }

  private onContextMenuClosed_() {
    this.contextMenuBookmark_ = undefined;
  }

  private showDeletionToastWithCount_(deletionCount: number) {
    PluralStringProxyImpl.getInstance()
        .getPluralString('bookmarkDeletionCount', deletionCount)
        .then(pluralString => {
          this.deletionDescription_ = pluralString;
          this.$.deletionToast.get().show();
        });
  }

  private showDisabledFeatureDialog_() {
    this.$.disabledFeatureDialog.showModal();
  }

  private closeDisabledFeatureDialog_() {
    this.$.disabledFeatureDialog.close();
  }

  private onUndoClicked_() {
    this.bookmarksApi_.undo();
    this.$.deletionToast.get().hide();
  }

  private onMoveClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    if (editingDisabledByPolicy(this.selectedBookmarks_)) {
      this.showDisabledFeatureDialog_();
      return;
    }
    this.showEditDialog_(this.selectedBookmarks_, true);
  }

  private showEditDialog_(
      bookmarks: chrome.bookmarks.BookmarkTreeNode[], moveOnly: boolean) {
    this.$.editDialog.showDialog(
        this.activeFolderPath_, this.bookmarksService_.getTopLevelBookmarks(),
        bookmarks, moveOnly);
  }

  private onEditMenuClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.$.contextMenu.showAt(
        event, this.selectedBookmarks_.slice(), false, false);
  }

  private onSortTypeClicked_(event: DomRepeatEvent<SortOption>) {
    event.preventDefault();
    event.stopPropagation();
    this.$.sortMenu.close();
    this.activeSortIndex_ = event.model.index;
    this.bookmarksApi_.setSortOrder(event.model.item.sortOrder);
    chrome.metricsPrivate.recordEnumerationValue(
        'PowerBookmarks.SidePanel.SortTypeShown', event.model.item.sortOrder,
        SortOrder.kCount);
  }

  private onVisualViewClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.$.sortMenu.close();
    this.compact_ = false;
    this.$.shownBookmarksIronList.notifyResize();
    this.bookmarksApi_.setViewType(ViewType.kExpanded);
    chrome.metricsPrivate.recordEnumerationValue(
        'PowerBookmarks.SidePanel.ViewTypeShown', ViewType.kExpanded,
        ViewType.kCount);
  }

  private onCompactViewClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.$.sortMenu.close();
    this.compact_ = true;
    this.$.shownBookmarksIronList.notifyResize();
    this.bookmarksApi_.setViewType(ViewType.kCompact);
    chrome.metricsPrivate.recordEnumerationValue(
        'PowerBookmarks.SidePanel.ViewTypeShown', ViewType.kCompact,
        ViewType.kCount);
  }

  private onAddTabClicked_() {
    const newParent = this.getParentFolder_();
    if (editingDisabledByPolicy([newParent])) {
      this.showDisabledFeatureDialog_();
      return;
    }
    chrome.metricsPrivate.recordUserAction(ADD_URL_ACTION_UMA);
    this.bookmarksApi_.bookmarkCurrentTabInFolder(newParent.id);
  }

  private hideAddTabButton_() {
    return this.editing_ || this.guestMode_;
  }

  private disableBackButton_(): boolean {
    return !this.activeFolderPath_.length || this.editing_;
  }

  private getEmptyTitle_(): string {
    if (this.guestMode_) {
      return loadTimeData.getString('emptyTitleGuest');
    } else if (this.shouldShowEmptySearchState_()) {
      return loadTimeData.getString('emptyTitleSearch');
    } else {
      return loadTimeData.getString('emptyTitle');
    }
  }

  private getEmptyBody_(): string {
    if (this.guestMode_) {
      return loadTimeData.getString('emptyBodyGuest');
    } else if (this.shouldShowEmptySearchState_()) {
      return loadTimeData.getString('emptyBodySearch');
    } else {
      return loadTimeData.getString('emptyBody');
    }
  }

  private getEmptyImagePath_(): string {
    return this.shouldShowEmptySearchState_() ? '' :
                                                './images/bookmarks_empty.svg';
  }

  private getEmptyImagePathDark_(): string {
    return this.shouldShowEmptySearchState_() ?
        '' :
        './images/bookmarks_empty_dark.svg';
  }

  /**
   * Whether the given price-tracked bookmark should display as if discounted.
   */
  private showDiscountedPrice_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      boolean {
    const bookmarkProductInfo = this.get(`trackedProductInfos_.${bookmark.id}`);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.previousPrice.length > 0;
    }
    return false;
  }

  private getCurrentPrice_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      string {
    const bookmarkProductInfo = this.get(`trackedProductInfos_.${bookmark.id}`);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.currentPrice;
    } else {
      return '';
    }
  }

  private getPreviousPrice_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      string {
    const bookmarkProductInfo = this.get(`trackedProductInfos_.${bookmark.id}`);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.previousPrice;
    } else {
      return '';
    }
  }

  private onShownBookmarksResize_() {
    // The iron-list of `shownBookmarks_` is in a dynamically sized card.
    // Any time the size changes, let iron-list know so that iron-list can
    // properly adjust to its possibly new height.
    this.$.shownBookmarksIronList.notifyResize();

    this.hasScrollbars_ =
        this.$.bookmarks.scrollHeight > this.$.bookmarks.offsetHeight;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'power-bookmarks-list': PowerBookmarksListElement;
  }
}

customElements.define(PowerBookmarksListElement.is, PowerBookmarksListElement);
