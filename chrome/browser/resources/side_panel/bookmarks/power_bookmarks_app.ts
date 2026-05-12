// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import './commerce/shopping_list.js';
import './icons.html.js';
import './power_bookmarks_context_menu.js';
import './power_bookmarks_labels.js';
import './power_bookmarks_edit_dialog.js';
import './power_bookmarks_list.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_empty_state.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_footer.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_icons.html.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_list_item_badge.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_shared_style.css.js';
import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '//resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import '//resources/cr_elements/cr_toolbar/cr_toolbar_selection_overlay.js';
import '//resources/cr_elements/icons.html.js';

import type {SpEmptyStateElement} from '//bookmarks-side-panel.top-chrome/shared/sp_empty_state.js';
import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import type {PriceTrackingBrowserProxy} from '//resources/cr_components/commerce/price_tracking_browser_proxy.js';
import {PriceTrackingBrowserProxyImpl} from '//resources/cr_components/commerce/price_tracking_browser_proxy.js';
import type {BookmarkProductInfo} from '//resources/cr_components/commerce/shared.mojom-webui.js';
import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrLazyRenderElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import type {CrToolbarSearchFieldElement} from '//resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ActionSource} from './bookmarks.mojom-webui.js';
import type {BookmarksTreeNode} from './bookmarks.mojom-webui.js';
import type {BookmarksApiProxy} from './bookmarks_api_proxy.js';
import {BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';
import {getTemplate} from './power_bookmarks_app.html.js';
import type {PowerBookmarksContextMenuElement} from './power_bookmarks_context_menu.js';
import type {PowerBookmarksEditDialogElement} from './power_bookmarks_edit_dialog.js';
import {TEMP_FOLDER_ID_PREFIX} from './power_bookmarks_edit_dialog.js';
import type {PowerBookmarksLabelsElement} from './power_bookmarks_labels.js';
import type {PowerBookmarksListElement} from './power_bookmarks_list.js';
import {recordBookmarkAdded, recordFolderAdded, recordSearchCTR, SearchAction} from './power_bookmarks_metrics.js';
import type {Label} from './power_bookmarks_service.js';
import {editingDisabledByPolicy, PowerBookmarksService} from './power_bookmarks_service.js';
import type {PowerBookmarksDelegate} from './power_bookmarks_service.js';

export interface PowerBookmarksAppElement {
  $: {
    bookmarksList: PowerBookmarksListElement,
    contextMenu: PowerBookmarksContextMenuElement,
    deletionToast: CrLazyRenderElement<CrToastElement>,
    powerBookmarksContainer: HTMLElement,
    searchField: CrToolbarSearchFieldElement,
    editDialog: PowerBookmarksEditDialogElement,
    disabledFeatureDialog: CrDialogElement,
    topLevelEmptyState: SpEmptyStateElement,
    footer: HTMLElement,
    labels: PowerBookmarksLabelsElement,
  };
}

interface AppSectionVisibility {
  search?: boolean;
  labels?: boolean;
  topLevelEmptyState?: boolean;
  footer?: boolean;
}

export class PowerBookmarksAppElement extends PolymerElement implements
    PowerBookmarksDelegate {
  static get is() {
    return 'power-bookmarks-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      contextMenuBookmark_: Object,

      activeFolderPath_: {
        type: Array,
        value: () => [],
      },

      currentUrl_: String,

      labels_: {
        type: Array,
        value: () => [],
      },

      editing_: {
        type: Boolean,
        value: false,
      },

      selectedBookmarks_: {
        type: Object,
        value: () => {
          return {};
        },
      },

      guestMode_: {
        type: Boolean,
        value: loadTimeData.getBoolean('guestMode'),
        reflectToAttribute: true,
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

      hasLoadedData_: {
        type: Boolean,
        value: false,
      },

      searchQuery_: String,

      trackedProductInfos_: {
        type: Object,
        value: () => {
          return {};
        },
      },

      hasSomeActiveFilter_: {
        type: Boolean,
        value: false,
        computed: 'computeHasSomeActiveFilter_(searchQuery_, labels_.*)',
      },

      hasShownBookmarks_: {
        type: Boolean,
        value: false,
      },

      sectionVisibility_: {
        type: Object,
        computed: 'computeSectionVisibility_(hasLoadedData_,' +
            'activeFolderPath_.length, hasShownBookmarks_,' +
            'labels_.length, hasSomeActiveFilter_)',
      },
    };
  }

  private bookmarksApi_: BookmarksApiProxy =
      BookmarksApiProxyImpl.getInstance();
  private priceTrackingProxy_: PriceTrackingBrowserProxy =
      PriceTrackingBrowserProxyImpl.getInstance();
  private shoppingListenerIds_: number[] = [];
  declare private trackedProductInfos_: {[key: string]: BookmarkProductInfo};
  private availableProductInfos_ = new Map<string, BookmarkProductInfo>();
  private bookmarksService_: PowerBookmarksService;
  declare private activeFolderPath_: BookmarksTreeNode[];
  declare private labels_: Label[];
  declare private searchQuery_: string|undefined;
  declare private currentUrl_: string|undefined;
  declare private editing_: boolean;
  declare private selectedBookmarks_: {[key: string]: boolean};
  declare private guestMode_: boolean;
  declare private deletionDescription_: string;
  declare private hasScrollbars_: boolean;
  declare private contextMenuBookmark_: BookmarksTreeNode|undefined;
  declare private hasLoadedData_: boolean;
  declare private hasSomeActiveFilter_: boolean;
  declare private hasShownBookmarks_: boolean;
  declare private sectionVisibility_: AppSectionVisibility;
  private recordCountMetricsOnNextUpdate_: boolean = false;
  private showUiCalled_: boolean = false;
  private visibilityChangedListener_: () => void = () => {
    if (document.visibilityState === 'hidden') {
      this.$.contextMenu.close();
    }
  };

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();

    const bookmarksService = new PowerBookmarksService(this);
    PowerBookmarksService.setInstance(bookmarksService);
    this.bookmarksService_ = PowerBookmarksService.getInstance();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setAttribute('role', 'application');
    if (!this.showUiCalled_) {
      this.showUiCalled_ = true;
      this.bookmarksApi_.showUi();
    }
    this.bookmarksService_.startListening();
    this.priceTrackingProxy_.getAllPriceTrackedBookmarkProductInfo().then(
        res => {
          const newTrackedProductInfos = {...this.trackedProductInfos_};
          res.productInfos.forEach(product => {
            newTrackedProductInfos[product.bookmarkId.toString()] = product;
          });
          this.trackedProductInfos_ = newTrackedProductInfos;
        });
    this.priceTrackingProxy_.getAllShoppingBookmarkProductInfo().then(res => {
      res.productInfos.forEach(
          product => this.setAvailableProductInfo_(product));
    });
    const callbackRouter = this.priceTrackingProxy_.getCallbackRouter();
    this.shoppingListenerIds_.push(
        callbackRouter.priceTrackedForBookmark.addListener(
            (product: BookmarkProductInfo) =>
                this.onBookmarkPriceTracked_(product)),
        callbackRouter.priceUntrackedForBookmark.addListener(
            (product: BookmarkProductInfo) =>
                this.onBookmarkPriceUntracked_(product)),
    );

    document.addEventListener(
        'visibilitychange', this.visibilityChangedListener_);
  }

  override disconnectedCallback() {
    this.bookmarksService_.stopListening();
    this.shoppingListenerIds_.forEach(
        id => this.priceTrackingProxy_.getCallbackRouter().removeListener(id));

    document.removeEventListener(
        'visibilitychange', this.visibilityChangedListener_);
  }

  getCurrentUrlForTesting(): string|undefined {
    return this.currentUrl_;
  }

  // PowerBookmarksDelegate
  setCurrentUrl(url: string) {
    this.currentUrl_ = url;
  }

  onBookmarksLoaded() {
    this.$.bookmarksList.onBookmarksLoaded();
    this.hasLoadedData_ = true;
  }

  onBookmarkChanged(id: string) {
    this.$.bookmarksList.onBookmarkChanged(id);
    this.updateShoppingData_();
  }

  onBookmarkAdded(bookmark: BookmarksTreeNode, parent: BookmarksTreeNode) {
    this.$.bookmarksList.onBookmarkAdded(bookmark, parent);
    this.updateShoppingData_();
  }

  onBookmarkMoved(
      bookmark: BookmarksTreeNode, oldParent: BookmarksTreeNode,
      newParent: BookmarksTreeNode) {
    this.$.bookmarksList.onBookmarkMoved(bookmark, oldParent, newParent);
  }

  onBookmarkRemoved(bookmark: BookmarksTreeNode) {
    if (this.$.contextMenu.anyBookmarkMatches(bookmark.id)) {
      this.$.contextMenu.close();
    }
    this.$.bookmarksList.onBookmarkRemoved(bookmark);
    const newTrackedProductInfos = {...this.trackedProductInfos_};
    delete newTrackedProductInfos[bookmark.id];
    this.trackedProductInfos_ = newTrackedProductInfos;
    this.availableProductInfos_.delete(bookmark.id);
    if (this.selectedBookmarks_[bookmark.id]) {
      this.set(`selectedBookmarks_.${bookmark.id}`, false);
    }
  }

  getTrackedProductInfos(): {[key: string]: BookmarkProductInfo} {
    return this.trackedProductInfos_;
  }

  getAvailableProductInfos(): Map<string, BookmarkProductInfo> {
    return this.availableProductInfos_;
  }

  getProductImageUrl(bookmark: BookmarksTreeNode): string {
    const bookmarkProductInfo = this.availableProductInfos_.get(bookmark.id);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.imageUrl;
    } else {
      return '';
    }
  }

  private onBookmarkPriceTracked_(product: BookmarkProductInfo) {
    this.trackedProductInfos_ = {
      ...this.trackedProductInfos_,
      [product.bookmarkId.toString()]: product,
    };
  }

  private onBookmarkPriceUntracked_(product: BookmarkProductInfo) {
    const newTrackedProductInfos = {...this.trackedProductInfos_};
    delete newTrackedProductInfos[product.bookmarkId.toString()];
    this.trackedProductInfos_ = newTrackedProductInfos;
  }

  private getActiveFolder_(): BookmarksTreeNode|undefined {
    if (this.activeFolderPath_.length) {
      return this.activeFolderPath_[this.activeFolderPath_.length - 1];
    }
    return undefined;
  }

  setImageUrl(bookmark: BookmarksTreeNode, url: string): void {
    this.$.bookmarksList.setImageUrl(bookmark, url);
  }

  private updateShoppingData_() {
    this.availableProductInfos_.clear();
    this.priceTrackingProxy_.getAllShoppingBookmarkProductInfo().then(res => {
      res.productInfos.forEach(
          product => this.setAvailableProductInfo_(product));
    });
  }

  private setAvailableProductInfo_(productInfo: BookmarkProductInfo) {
    const bookmarkId = productInfo.bookmarkId.toString();
    this.availableProductInfos_.set(bookmarkId, productInfo);
    if (productInfo.info.imageUrl === '') {
      return;
    }
    const bookmark = this.bookmarksService_.findBookmarkWithId(bookmarkId)!;
    if (!bookmark) {
      return;
    }
    this.$.bookmarksList.setImageUrl(bookmark, productInfo.info.imageUrl);
  }

  private canAddCurrentUrl_(): boolean {
    return this.bookmarksService_.canAddUrl(
        this.currentUrl_, this.getActiveFolder_());
  }

  private async onBookmarksEdited_(event: CustomEvent<{
    bookmarks: BookmarksTreeNode[],
    name: string|undefined,
    url: string|undefined,
    folderId: string,
    newFolders: BookmarksTreeNode[],
  }>) {
    event.preventDefault();
    event.stopPropagation();
    let parentId = event.detail.folderId;
    for (const folder of event.detail.newFolders) {
      recordFolderAdded();
      const result: {newFolderId: string} =
          await this.bookmarksApi_.createFolder(folder.parentId, folder.title);
      folder.children!.forEach(child => child.parentId = result.newFolderId);
      if (folder.id === parentId) {
        parentId = result.newFolderId;
      }
      // Removing folders added in edit menu while editing a bookmark as they
      // are made with TEMP_FOLDER_ID_PREFIX bookmark-id and are again created
      // with correct id with createFolder method above
      const parentFolder =
          this.bookmarksService_.findBookmarkWithId(folder.parentId)!;
      parentFolder.children = parentFolder.children!.filter(
          child => !child.id.startsWith(TEMP_FOLDER_ID_PREFIX));
    }
    this.bookmarksApi_.editBookmarks(
        event.detail.bookmarks.map(bookmark => bookmark.id), event.detail.name,
        event.detail.url, parentId);
    this.selectedBookmarks_ = {};
    this.editing_ = false;
  }

  private getSelectedDescription_() {
    return loadTimeData.getStringF(
        'selectedBookmarkCount', this.getSelectedBookmarksLength_());
  }

  private getSelectedBookmarksList_(): BookmarksTreeNode[] {
    const selectedEntries = Object.entries(this.selectedBookmarks_)
                                .filter(([_id, selected]) => selected);
    const selectedIds = selectedEntries.map(([id, _selected]) => id);
    return selectedIds
        .map((id) => this.bookmarksService_.findBookmarkWithId(id)!)
        .filter(b => !!b);
  }

  private getSelectedBookmarksLength_(): number {
    return Object.values(this.selectedBookmarks_)
        .filter((selected) => selected)
        .length;
  }

  /**
   * Toggles the given label between active and inactive.
   */
  private onLabelsChanged_() {
    this.labels_ = [...this.$.labels.labels];
  }

  private onSearchChanged_(e: CustomEvent<string>) {
    this.recordCountMetricsOnNextUpdate_ = true;
    this.searchQuery_ = e.detail.toLocaleLowerCase();
  }

  private onSearchBlurred_() {
    recordSearchCTR(SearchAction.SEARCHED);
  }

  private onContextMenuShown_(bookmark: BookmarksTreeNode) {
    this.contextMenuBookmark_ = bookmark;
  }

  private onShowContextMenuClicked_(
      event: CustomEvent<{bookmark: BookmarksTreeNode, event: MouseEvent}>) {
    event.preventDefault();
    event.stopPropagation();
    if (!event.detail.bookmark) {
      return;
    }
    const priceTracked =
        !!this.bookmarksService_.getPriceTrackedInfo(event.detail.bookmark);
    const priceTrackingEligible =
        !!this.bookmarksService_.getAvailableProductInfo(event.detail.bookmark);
    const bookmark = event.detail.bookmark;
    const target = event.detail.event.target as HTMLElement;
    Promise
        .all([
          this.bookmarksApi_.isActiveTabInSplit(),
          this.bookmarksApi_.getIncognitoAvailableCount([bookmark.id]),
        ])
        .then(([isSplit, incognito]) => {
          if (event.detail.event.button === 0) {
            this.$.contextMenu.showAt(
                target, [bookmark], priceTracked, priceTrackingEligible,
                isSplit, incognito.incognitoCount,
                this.onContextMenuShown_.bind(this, bookmark));
          } else {
            this.$.contextMenu.showAtPosition(
                event.detail.event, [bookmark], priceTracked,
                priceTrackingEligible, isSplit, incognito.incognitoCount,
                this.onContextMenuShown_.bind(this, bookmark));
          }
        });
  }

  private onBulkEditClick_() {
    this.editing_ = !this.editing_;
    if (!this.editing_) {
      this.selectedBookmarks_ = {};
    }
  }

  private onDeleteClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    const selectedBookmarksList = this.getSelectedBookmarksList_();
    if (editingDisabledByPolicy(selectedBookmarksList)) {
      this.showDisabledFeatureDialog_();
      return;
    }
    this.bookmarksApi_
        .deleteBookmarks(selectedBookmarksList.map((bookmark) => bookmark.id))
        .then(() => {
          this.showDeletionToast_(selectedBookmarksList);
          this.selectedBookmarks_ = {};
          this.editing_ = false;
        });
  }

  private onContextMenuEditClicked_(
      event: CustomEvent<{bookmarks: BookmarksTreeNode[]}>) {
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
      event: CustomEvent<{bookmarks: BookmarksTreeNode[]}>) {
    event.preventDefault();
    event.stopPropagation();
    this.showDeletionToast_(event.detail.bookmarks);
    this.selectedBookmarks_ = {};
    this.editing_ = false;
  }

  private onContextMenuClosed_() {
    // This check is needed to avoid the case where the context menu is closed
    // via right-click a new row, and is already re-opened by the time this
    // executes.
    if (!this.$.contextMenu.isOpen()) {
      this.contextMenuBookmark_ = undefined;
    }
  }

  private onClearSearch_() {
    this.$.searchField.setValue('');
  }

  private countDescendants_(node: BookmarksTreeNode): number {
    let count = 0;
    if (node.url) {
      count++;
    }
    if (node.children) {
      for (const child of node.children) {
        count += this.countDescendants_(child);
      }
    }
    return count;
  }

  private showDeletionToast_(bookmarks: BookmarksTreeNode[]) {
    const totalCount = bookmarks.reduce(
        (prev, curr) => prev + this.countDescendants_(curr), 0);

    PluralStringProxyImpl.getInstance()
        .getPluralString('bookmarkDeletionCount', totalCount)
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
    const selectedBookmarksList = this.getSelectedBookmarksList_();
    if (editingDisabledByPolicy(selectedBookmarksList)) {
      this.showDisabledFeatureDialog_();
      return;
    }
    this.showEditDialog_(selectedBookmarksList, true);
  }

  private showEditDialog_(bookmarks: BookmarksTreeNode[], moveOnly: boolean) {
    if (!loadTimeData.getBoolean('isBookmarksMigrationUiChanges')) {
      this.$.editDialog.showDialog(
          this.activeFolderPath_, this.bookmarksService_.getTopLevelBookmarks(),
          bookmarks, moveOnly);
      return;
    }

    if (moveOnly) {
      this.bookmarksApi_.contextMenuMove(
          bookmarks.map(bookmark => bookmark.id), ActionSource.kBookmark);
    } else {
      this.bookmarksApi_.contextMenuEdit(
          bookmarks.map(bookmark => bookmark.id), ActionSource.kBookmark);
    }
  }

  private onBulkEditMenuClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    const target = event.target as HTMLElement;
    const selectedBookmarks = this.getSelectedBookmarksList_();
    const ids = selectedBookmarks.map(b => b.id);
    Promise
        .all([
          this.bookmarksApi_.isActiveTabInSplit(),
          this.bookmarksApi_.getIncognitoAvailableCount(ids),
        ])
        .then(([isSplit, incognito]) => {
          this.$.contextMenu.showAt(
              target, selectedBookmarks, false, false, isSplit,
              incognito.incognitoCount);
        });
  }

  private onAddTabClicked_() {
    const newParent = this.$.bookmarksList.getParentFolder();
    if (editingDisabledByPolicy([newParent])) {
      this.showDisabledFeatureDialog_();
      return;
    }
    recordBookmarkAdded();
    this.bookmarksApi_.bookmarkCurrentTabInFolder(newParent.id);
  }

  private onRowSelectedChange_(
      event: CustomEvent<{id: string, checked: boolean}>) {
    this.set(`selectedBookmarks_.${event.detail.id}`, event.detail.checked);
  }

  private hideAddTabButton_() {
    return this.editing_ || this.guestMode_;
  }

  private getEmptyTitle_(): string {
    if (this.guestMode_) {
      return loadTimeData.getString('emptyTitleGuest');
    } else if (this.hasSomeActiveFilter_) {
      return loadTimeData.getString('emptyTitleSearch');
    } else {
      return loadTimeData.getString('emptyTitle');
    }
  }

  private getEmptyBody_(): string {
    if (this.guestMode_) {
      return loadTimeData.getString('emptyBodyGuest');
    } else if (this.hasSomeActiveFilter_) {
      return loadTimeData.getString('emptyBodySearch');
    } else {
      return loadTimeData.getString('emptyBody');
    }
  }

  private getEmptyImagePath_(): string {
    return this.hasSomeActiveFilter_ ? '' : './images/bookmarks_empty.svg';
  }

  private getEmptyImagePathDark_(): string {
    return this.hasSomeActiveFilter_ ? '' : './images/bookmarks_empty_dark.svg';
  }

  private computeHasSomeActiveFilter_(): boolean {
    return !!this.searchQuery_ || this.labels_.some(label => label.active);
  }

  private computeSectionVisibility_(): AppSectionVisibility {
    if (this.guestMode_) {
      return {topLevelEmptyState: true};
    }

    if (!this.hasLoadedData_) {
      return {search: true, footer: true};
    }

    const hasActiveFolder = this.activeFolderPath_.length > 0;
    const hasShownBookmarks = this.hasShownBookmarks_;
    const hasSomeActiveFilter = this.hasSomeActiveFilter_;

    return {
      search: true,
      labels: this.labels_.length > 0,
      topLevelEmptyState:
          !hasShownBookmarks && (hasSomeActiveFilter || !hasActiveFolder),
      footer: !hasSomeActiveFilter,
    };
  }

  private onHasScrollbarsChanged_(e: CustomEvent<boolean>) {
    this.hasScrollbars_ = e.detail;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'power-bookmarks-app': PowerBookmarksAppElement;
  }
}

customElements.define(PowerBookmarksAppElement.is, PowerBookmarksAppElement);
