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
import '//bookmarks-side-panel.top-chrome/shared/sp_list_item_badge.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
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
import {CrLazyRenderElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import {listenOnce} from '//resources/js/util_ts.js';
import {DomRepeatEvent, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ActionSource} from './bookmarks.mojom-webui.js';
import {BookmarksApiProxy, BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';
import {PowerBookmarksContextMenuElement} from './power_bookmarks_context_menu.js';
import {PowerBookmarksEditDialogElement} from './power_bookmarks_edit_dialog.js';
import {getTemplate} from './power_bookmarks_list.html.js';
import {Label, PowerBookmarksService} from './power_bookmarks_service.js';

function getBookmarkName(bookmark: chrome.bookmarks.BookmarkTreeNode): string {
  return bookmark.title || bookmark.url || '';
}

export interface PowerBookmarksListElement {
  $: {
    contextMenu: CrLazyRenderElement<PowerBookmarksContextMenuElement>,
    deletionToast: CrLazyRenderElement<CrToastElement>,
    powerBookmarksContainer: HTMLElement,
    sortMenu: CrActionMenuElement,
    editDialog: PowerBookmarksEditDialogElement,
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
        value: true,
      },

      activeFolderPath_: {
        type: Array,
        value: () => [],
      },

      labels_: {
        type: Array,
        value: () => [{
          label: loadTimeData.getString('priceTrackingLabel'),
          icon: 'bookmarks:price-tracking',
          active: false,
        }],
      },

      activeSortIndex_: {
        type: Number,
        value: 0,
      },

      sortTypes_: {
        type: Array,
        value: () =>
            [loadTimeData.getString('sortNewest'),
             loadTimeData.getString('sortOldest'),
             loadTimeData.getString('sortAlphabetically'),
             loadTimeData.getString('sortReverseAlphabetically')],
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

      renamingParentId_: {
        type: String,
        value: '',
      },

      renamingId_: {
        type: String,
        value: '',
      },

      deletionDescription_: {
        type: String,
        value: '',
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
  private shownBookmarks_: chrome.bookmarks.BookmarkTreeNode[];
  private bookmarksService_: PowerBookmarksService =
      new PowerBookmarksService(this);
  private compact_: boolean;
  private activeFolderPath_: chrome.bookmarks.BookmarkTreeNode[];
  private labels_: Label[];
  private compactDescriptions_ = new Map<string, string>();
  private expandedDescriptions_ = new Map<string, string>();
  private activeSortIndex_: number;
  private sortTypes_: string[];
  private searchQuery_: string|undefined;
  private currentUrl_: string|undefined;
  private editing_: boolean;
  private selectedBookmarks_: chrome.bookmarks.BookmarkTreeNode[];
  private guestMode_: boolean;
  private renamingParentId_: string;
  private renamingId_: string;
  private deletionDescription_: string;

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
  }

  override disconnectedCallback() {
    this.bookmarksService_.stopListening();
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
  }

  onBookmarkCreated(
      bookmark: chrome.bookmarks.BookmarkTreeNode,
      parent: chrome.bookmarks.BookmarkTreeNode) {
    if (this.visibleParent_(parent)) {
      this.shownBookmarks_.unshift(bookmark);
      this.bookmarksService_.sortBookmarks(
          this.shownBookmarks_, this.activeSortIndex_);
      this.shownBookmarks_ = this.shownBookmarks_.slice();
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkCreated', getBookmarkName(bookmark)));
    }
    if (parent.id === this.renamingParentId_) {
      this.renamingParentId_ = '';
      this.renamingId_ = bookmark.id;
    }
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
   * root folder while the all bookmarks list is shown.
   */
  private visibleParent_(parent: chrome.bookmarks.BookmarkTreeNode): boolean {
    const activeFolder = this.getActiveFolder_();
    return (!activeFolder && parent.parentId === '0') ||
        parent === activeFolder;
  }

  private getActiveFolder_(): chrome.bookmarks.BookmarkTreeNode|undefined {
    if (this.activeFolderPath_.length) {
      return this.activeFolderPath_[this.activeFolderPath_.length - 1];
    }
    return undefined;
  }

  private getBookmarksListRole_(): string {
    return this.editing_ ? 'listbox' : 'list';
  }

  private getBookmarkName_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      string {
    return bookmark.title || bookmark.url || '';
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
    return this.sortTypes_[this.activeSortIndex_]!;
  }

  private renamingItem_(id: string) {
    return id === this.renamingId_;
  }

  private isPriceTracked_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      boolean {
    return this.bookmarksService_.getProductInfo(bookmark) !== undefined;
  }

  /**
   * Update the list of bookmarks and folders displayed to the user.
   */
  private updateShownBookmarks_() {
    this.shownBookmarks_ = this.bookmarksService_.filterBookmarks(
        this.getActiveFolder_(), this.activeSortIndex_, this.searchQuery_,
        this.labels_);
  }

  private canAddCurrentUrl_(): boolean {
    return this.bookmarksService_.canAddUrl(
        this.currentUrl_, this.getActiveFolder_());
  }

  private canEdit_(bookmark: chrome.bookmarks.BookmarkTreeNode): boolean {
    return bookmark.id !== loadTimeData.getString('bookmarksBarId');
  }

  private getSortMenuItemLabel_(sortType: string): string {
    return loadTimeData.getStringF('sortByType', sortType);
  }

  private sortMenuItemIsSelected_(sortType: string): boolean {
    return this.sortTypes_[this.activeSortIndex_] === sortType;
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

  private onShowContextMenuClicked_(
      event: CustomEvent<
          {bookmark: chrome.bookmarks.BookmarkTreeNode, event: MouseEvent}>) {
    event.preventDefault();
    event.stopPropagation();
    if (event.detail.event.button === 0) {
      this.$.contextMenu.get().showAt(
          event.detail.event, [event.detail.bookmark]);
    } else {
      this.$.contextMenu.get().showAtPosition(
          event.detail.event, [event.detail.bookmark]);
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
    this.renamingParentId_ = newParent.id;
    this.bookmarksApi_.createFolder(
        newParent.id, loadTimeData.getString('newFolderTitle'));
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
    this.bookmarksApi_
        .deleteBookmarks(this.selectedBookmarks_.map(bookmark => bookmark.id))
        .then(() => {
          this.showDeletionToastWithCount_(this.selectedBookmarks_.length);
          this.selectedBookmarks_ = [];
          this.editing_ = false;
        });
  }

  private onContextMenuDeleteClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    // Context menu delete is expected to only be called on a single bookmark.
    this.showDeletionToastWithCount_(1);
  }

  private showDeletionToastWithCount_(deletionCount: number) {
    PluralStringProxyImpl.getInstance()
        .getPluralString('bookmarkDeletionCount', deletionCount)
        .then(pluralString => {
          this.deletionDescription_ = pluralString;
          this.$.deletionToast.get().show();
        });
  }

  private onUndoClicked_() {
    this.bookmarksApi_.undo();
    this.$.deletionToast.get().hide();
  }

  private onMoveClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.$.editDialog.showDialog(
        this.getActiveFolder_(), this.bookmarksService_.getTopLevelBookmarks(),
        this.selectedBookmarks_);
  }

  private onEditMenuClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.$.contextMenu.get().showAt(event, this.selectedBookmarks_.slice());
  }

  private onSortTypeClicked_(event: DomRepeatEvent<string>) {
    event.preventDefault();
    event.stopPropagation();
    this.$.sortMenu.close();
    this.activeSortIndex_ = event.model.index;
  }

  private onVisualViewClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.$.sortMenu.close();
    this.compact_ = false;
  }

  private onCompactViewClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.$.sortMenu.close();
    this.compact_ = true;
  }

  private onAddTabClicked_() {
    const newParent = this.getParentFolder_();
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
    } else {
      return loadTimeData.getString('emptyTitle');
    }
  }

  private getEmptyBody_(): string {
    if (this.guestMode_) {
      return loadTimeData.getString('emptyBodyGuest');
    } else {
      return loadTimeData.getString('emptyBody');
    }
  }

  /**
   * Whether the given price-tracked bookmark should display as if discounted.
   */
  private showDiscountedPrice_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      boolean {
    const bookmarkProductInfo = this.bookmarksService_.getProductInfo(bookmark);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.previousPrice.length > 0;
    }
    return false;
  }

  private getCurrentPrice_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      string {
    const bookmarkProductInfo = this.bookmarksService_.getProductInfo(bookmark);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.currentPrice;
    } else {
      return '';
    }
  }

  private getPreviousPrice_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      string {
    const bookmarkProductInfo = this.bookmarksService_.getProductInfo(bookmark);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.previousPrice;
    } else {
      return '';
    }
  }

  private shouldPinFooter_(): boolean {
    return this.shownBookmarks_.length > 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'power-bookmarks-list': PowerBookmarksListElement;
  }
}

customElements.define(PowerBookmarksListElement.is, PowerBookmarksListElement);
