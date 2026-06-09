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
import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
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
import type {CrLazyRenderLitElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import type {CrToolbarSearchFieldElement} from '//resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {ActionSource} from './bookmarks.mojom-webui.js';
import type {BookmarksTreeNode} from './bookmarks.mojom-webui.js';
import type {BookmarksApiProxy} from './bookmarks_api_proxy.js';
import {BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';
import {getCss} from './power_bookmarks_app.css.js';
import {getHtml} from './power_bookmarks_app.html.js';
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
    deletionToast: CrLazyRenderLitElement<CrToastElement>,
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
  heading?: boolean;
}

export class PowerBookmarksAppElement extends CrLitElement implements
    PowerBookmarksDelegate {
  static get is() {
    return 'power-bookmarks-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      contextMenuBookmark_: {type: Object},
      activeFolderPath_: {type: Array},
      currentUrl_: {type: String},
      labels_: {type: Array},
      editing_: {type: Boolean},
      renamingId_: {type: String},
      selectedBookmarks_: {type: Object},
      guestMode_: {
        type: Boolean,
        reflect: true,
      },
      canAddCurrentUrl_: {type: Boolean},
      deletionDescription_: {type: String},
      hasScrollbars_: {
        type: Boolean,
        reflect: true,
      },
      hasLoadedData_: {type: Boolean},
      searchQuery_: {type: String},
      trackedProductInfos_: {type: Object},
      hasSomeActiveFilter_: {type: Boolean},
      hasShownBookmarks_: {type: Boolean},
      sectionVisibility_: {type: Object},
    };
  }

  private bookmarksApi_: BookmarksApiProxy =
      BookmarksApiProxyImpl.getInstance();
  private priceTrackingProxy_: PriceTrackingBrowserProxy =
      PriceTrackingBrowserProxyImpl.getInstance();
  private shoppingListenerIds_: number[] = [];
  protected accessor trackedProductInfos_:
      {[key: string]: BookmarkProductInfo} = {};
  private availableProductInfos_ = new Map<string, BookmarkProductInfo>();
  private bookmarksService_: PowerBookmarksService;
  protected accessor activeFolderPath_: BookmarksTreeNode[] = [];
  protected accessor canAddCurrentUrl_: boolean = false;
  protected accessor labels_: Label[] = [];
  protected accessor searchQuery_: string = '';
  protected accessor currentUrl_: string|undefined = undefined;
  protected accessor editing_: boolean = false;
  protected accessor renamingId_: string = '';
  protected accessor selectedBookmarks_: {[key: string]: boolean} = {};
  protected accessor guestMode_: boolean = loadTimeData.getBoolean('guestMode');
  protected accessor deletionDescription_: string = '';
  protected accessor hasScrollbars_: boolean = false;
  protected accessor contextMenuBookmark_: BookmarksTreeNode|undefined =
      undefined;
  protected accessor hasLoadedData_: boolean = false;
  protected accessor hasSomeActiveFilter_: boolean = false;
  protected accessor hasShownBookmarks_: boolean = false;
  protected accessor sectionVisibility_: AppSectionVisibility = {};
  private showUiCalled_: boolean = false;


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
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.bookmarksService_.stopListening();
    this.shoppingListenerIds_.forEach(
        id => this.priceTrackingProxy_.getCallbackRouter().removeListener(id));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('currentUrl_')) {
      this.updateCanAddCurrentUrl_();
    }

    if (changedPrivateProperties.has('searchQuery_') ||
        changedPrivateProperties.has('labels_')) {
      this.hasSomeActiveFilter_ = this.computeHasSomeActiveFilter_();
    }

    if (changedPrivateProperties.has('hasLoadedData_') ||
        changedPrivateProperties.has('activeFolderPath_') ||
        changedPrivateProperties.has('hasShownBookmarks_') ||
        changedPrivateProperties.has('labels_') ||
        changedPrivateProperties.has('searchQuery_') ||
        changedPrivateProperties.has('hasSomeActiveFilter_') ||
        changedPrivateProperties.has('guestMode_')) {
      this.sectionVisibility_ = this.computeSectionVisibility_();
    }
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
    this.updateCanAddCurrentUrl_();
  }

  onBookmarkChanged(id: string) {
    this.$.bookmarksList.onBookmarkChanged(id);
    this.updateShoppingData_();
    this.updateCanAddCurrentUrl_();
  }

  onBookmarkAdded(bookmark: BookmarksTreeNode, parent: BookmarksTreeNode) {
    this.$.bookmarksList.onBookmarkAdded(bookmark, parent);
    this.updateShoppingData_();
    this.updateCanAddCurrentUrl_();
  }

  onBookmarkMoved(
      bookmark: BookmarksTreeNode, oldParent: BookmarksTreeNode,
      newParent: BookmarksTreeNode) {
    this.$.bookmarksList.onBookmarkMoved(bookmark, oldParent, newParent);
    this.updateCanAddCurrentUrl_();
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
      const newSelectedBookmarks = {...this.selectedBookmarks_};
      delete newSelectedBookmarks[bookmark.id];
      this.selectedBookmarks_ = newSelectedBookmarks;
    }
    this.updateCanAddCurrentUrl_();
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

  private updateCanAddCurrentUrl_() {
    this.canAddCurrentUrl_ = this.bookmarksService_.canAddUrl(
        this.currentUrl_, this.getActiveFolder_());
  }

  protected async onEditDialogSave_(event: CustomEvent<{
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

  protected getSelectedDescription_() {
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

  protected getSelectedBookmarksLength_(): number {
    return Object.values(this.selectedBookmarks_)
        .filter((selected) => selected)
        .length;
  }

  protected onLabelsChanged_(e: CustomEvent<{value: Label[]}>) {
    this.labels_ = e.detail.value;
  }

  protected onSearchChanged_(e: CustomEvent<string>) {
    this.searchQuery_ = e.detail.toLocaleLowerCase();
  }

  protected onSearchBlur_() {
    recordSearchCTR(SearchAction.SEARCHED);
  }

  private onContextMenuShown_(bookmark: BookmarksTreeNode) {
    this.contextMenuBookmark_ = bookmark;
  }

  protected onShowContextMenu_(
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

  protected toggleEditing_() {
    this.editing_ = !this.editing_;
    if (!this.editing_) {
      this.selectedBookmarks_ = {};
    }
  }

  protected onBookmarksListBulkEdit_() {
    this.toggleEditing_();
  }

  protected onClearSelectedItems_() {
    this.toggleEditing_();
  }

  protected onDeleteClick_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    const selectedBookmarksList = this.getSelectedBookmarksList_();
    if (editingDisabledByPolicy(selectedBookmarksList)) {
      this.onDisabledFeature_();
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

  protected onContextMenuEditClicked_(
      event: CustomEvent<{bookmarks: BookmarksTreeNode[]}>) {
    event.preventDefault();
    event.stopPropagation();
    if (editingDisabledByPolicy(event.detail.bookmarks)) {
      this.onDisabledFeature_();
      return;
    }
    this.showEditDialog_(
        event.detail.bookmarks, event.detail.bookmarks.length > 1);
  }

  protected onContextMenuDeleteClicked_(
      event: CustomEvent<{bookmarks: BookmarksTreeNode[]}>) {
    event.preventDefault();
    event.stopPropagation();
    this.showDeletionToast_(event.detail.bookmarks);
    this.selectedBookmarks_ = {};
    this.editing_ = false;
  }

  protected onContextMenuClose_() {
    // This check is needed to avoid the case where the context menu is closed
    // via right-click a new row, and is already re-opened by the time this
    // executes.
    if (!this.$.contextMenu.isOpen()) {
      this.contextMenuBookmark_ = undefined;
    }
  }

  protected onClearSearch_() {
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

  protected onDisabledFeature_() {
    this.$.disabledFeatureDialog.showModal();
  }

  protected onCloseDisabledFeatureDialogClick_() {
    this.$.disabledFeatureDialog.close();
  }

  protected onUndoClick_() {
    this.bookmarksApi_.undo();
    this.$.deletionToast.get().hide();
  }

  protected onMoveClick_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    const selectedBookmarksList = this.getSelectedBookmarksList_();
    if (editingDisabledByPolicy(selectedBookmarksList)) {
      this.onDisabledFeature_();
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

  protected onBulkEditMenuClick_(event: MouseEvent) {
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

  protected onAddTabClick_() {
    const newParent = this.$.bookmarksList.getParentFolder();
    if (editingDisabledByPolicy([newParent])) {
      this.onDisabledFeature_();
      return;
    }
    recordBookmarkAdded();
    this.bookmarksApi_.bookmarkCurrentTabInFolder(newParent.id);
  }

  protected onRowSelectedChange_(
      event: CustomEvent<{id: string, checked: boolean}>) {
    this.selectedBookmarks_ = {
      ...this.selectedBookmarks_,
      [event.detail.id]: event.detail.checked,
    };
  }

  protected hideAddTabButton_() {
    return this.editing_ || this.guestMode_;
  }

  protected getEmptyTitle_(): string {
    if (this.guestMode_) {
      return loadTimeData.getString('emptyTitleGuest');
    } else if (this.hasSomeActiveFilter_) {
      return loadTimeData.getString('emptyTitleSearch');
    } else {
      return loadTimeData.getString('emptyTitle');
    }
  }

  protected getEmptyBody_(): string {
    if (this.guestMode_) {
      return loadTimeData.getString('emptyBodyGuest');
    } else if (this.hasSomeActiveFilter_) {
      return loadTimeData.getString('emptyBodySearch');
    } else {
      return loadTimeData.getString('emptyBody');
    }
  }

  protected getEmptyImagePath_(): string {
    return this.hasSomeActiveFilter_ ? '' : './images/bookmarks_empty.svg';
  }

  protected getEmptyImagePathDark_(): string {
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
      heading: !hasSomeActiveFilter && (hasActiveFolder || hasShownBookmarks),
    };
  }

  protected onRenamingIdChanged_(e: CustomEvent<{value: string}>) {
    this.renamingId_ = e.detail.value;
  }

  protected onHasScrollbarsChanged_(e: CustomEvent<boolean>) {
    this.hasScrollbars_ = e.detail;
  }

  protected onContextMenuRenameClicked_(event: CustomEvent<{id: string}>) {
    this.renamingId_ = event.detail.id;
  }

  protected onHasShownBookmarksChanged_(e: CustomEvent<{value: boolean}>) {
    this.hasShownBookmarks_ = e.detail.value;
  }

  protected onActiveFolderPathChanged_(
      e: CustomEvent<{value: BookmarksTreeNode[]}>) {
    this.activeFolderPath_ = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'power-bookmarks-app': PowerBookmarksAppElement;
  }
}

customElements.define(PowerBookmarksAppElement.is, PowerBookmarksAppElement);
