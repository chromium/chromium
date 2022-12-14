// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import './commerce/shopping_list.js';
import './icons.html.js';
import './power_bookmark_chip.js';
import './power_bookmarks_context_menu.js';
import './power_bookmark_row.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import '//resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import '//resources/cr_elements/icons.html.js';

import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLazyRenderElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import {listenOnce} from '//resources/js/util_ts.js';
import {DomRepeatEvent, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ActionSource} from './bookmarks.mojom-webui.js';
import {BookmarksApiProxy, BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';
import {BookmarkProductInfo} from './commerce/shopping_list.mojom-webui.js';
import {ShoppingListApiProxy, ShoppingListApiProxyImpl} from './commerce/shopping_list_api_proxy.js';
import {PowerBookmarksContextMenuElement} from './power_bookmarks_context_menu.js';
import {getTemplate} from './power_bookmarks_list.html.js';

function getBookmarkName(bookmark: chrome.bookmarks.BookmarkTreeNode): string {
  return bookmark.title || bookmark.url || '';
}

interface Label {
  label: string;
  icon: string;
  active: boolean;
}

export interface PowerBookmarksListElement {
  $: {
    contextMenu: CrLazyRenderElement<PowerBookmarksContextMenuElement>,
    powerBookmarksContainer: HTMLElement,
    sortMenu: CrActionMenuElement,
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
      folders_: {
        type: Array,
        value: () => [],
      },

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

      showPriceTracking_: {
        type: Boolean,
        value: false,
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
    };
  }

  static get observers() {
    return [
      'updateShownBookmarks_(folders_.*, ' +
          'activeFolderPath_.*, labels_.*, activeSortIndex_, searchQuery_)',
    ];
  }

  private folders_: chrome.bookmarks.BookmarkTreeNode[];
  private shownBookmarks_: chrome.bookmarks.BookmarkTreeNode[];
  private bookmarksApi_: BookmarksApiProxy =
      BookmarksApiProxyImpl.getInstance();
  private shoppingListApi_: ShoppingListApiProxy =
      ShoppingListApiProxyImpl.getInstance();
  private listeners_ = new Map<string, Function>();
  private productInfos_ = new Map<string, BookmarkProductInfo>();
  private shoppingListenerIds_: number[] = [];
  private compact_: boolean;
  private activeFolderPath_: chrome.bookmarks.BookmarkTreeNode[];
  private labels_: Label[];
  private compactDescriptions_ = new Map<string, string>();
  private expandedDescriptions_ = new Map<string, string>();
  private showPriceTracking_: boolean;
  private activeSortIndex_: number;
  private sortTypes_: string[];
  private searchQuery_: string|undefined;
  private currentUrl_: string|undefined;
  private editing_: boolean;
  private selectedBookmarks_: chrome.bookmarks.BookmarkTreeNode[];
  private guestMode_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    this.setAttribute('role', 'tree');
    listenOnce(this.$.powerBookmarksContainer, 'dom-change', () => {
      setTimeout(() => this.bookmarksApi_.showUi(), 0);
    });
    this.bookmarksApi_.getFolders().then(folders => {
      this.folders_ = folders;
      this.folders_.forEach(bookmark => {
        this.findBookmarkDescriptions_(bookmark, true);
      });
      this.addListener_(
          'onChanged',
          (id: string, changedInfo: chrome.bookmarks.ChangeInfo) =>
              this.onChanged_(id, changedInfo));
      this.addListener_(
          'onCreated',
          (_id: string, node: chrome.bookmarks.BookmarkTreeNode) =>
              this.onCreated_(node));
      this.addListener_(
          'onMoved',
          (_id: string, movedInfo: chrome.bookmarks.MoveInfo) =>
              this.onMoved_(movedInfo));
      this.addListener_('onRemoved', (id: string) => this.onRemoved_(id));
      this.addListener_('onTabActivated', (_info: chrome.tabs.ActiveInfo) => {
        this.bookmarksApi_.getActiveUrl().then(url => this.currentUrl_ = url);
      });
      this.addListener_(
          'onTabUpdated',
          (_tabId: number, _changeInfo: object, tab: chrome.tabs.Tab) => {
            if (tab.active) {
              this.currentUrl_ = tab.url;
            }
          });
    });
    this.shoppingListApi_.getAllPriceTrackedBookmarkProductInfo().then(res => {
      res.productInfos.forEach(
          product =>
              this.productInfos_.set(product.bookmarkId.toString(), product));
      if (this.productInfos_.size > 0) {
        this.showPriceTracking_ = true;
        chrome.metricsPrivate.recordUserAction(
            'Commerce.PriceTracking.SidePanel.TrackedProductsShown');
      }
    });
    this.bookmarksApi_.getActiveUrl().then(url => this.currentUrl_ = url);
    const callbackRouter = this.shoppingListApi_.getCallbackRouter();
    this.shoppingListenerIds_.push(
        callbackRouter.priceTrackedForBookmark.addListener(
            () => this.onBookmarkPriceTracked()),
    );
  }

  override disconnectedCallback() {
    for (const [eventName, callback] of this.listeners_.entries()) {
      this.bookmarksApi_.callbackRouter[eventName]!.removeListener(callback);
    }
    this.shoppingListenerIds_.forEach(
        id => this.shoppingListApi_.getCallbackRouter().removeListener(id));
  }

  private addListener_(eventName: string, callback: Function): void {
    this.bookmarksApi_.callbackRouter[eventName]!.addListener(callback);
    this.listeners_.set(eventName, callback);
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

  private onChanged_(id: string, changedInfo: chrome.bookmarks.ChangeInfo) {
    const path = this.findPathToId_(id);
    const bookmark = path[path.length - 1]!;
    Object.assign(bookmark, changedInfo);
    this.findBookmarkDescriptions_(bookmark, false);
    Object.keys(changedInfo).forEach(key => {
      const visibleIndex = this.visibleIndex_(id);
      if (visibleIndex > -1) {
        this.notifyPath(`shownBookmarks_.${visibleIndex}.${key}`);
      }
    });
  }

  private onCreated_(node: chrome.bookmarks.BookmarkTreeNode) {
    const pathToParent = this.findPathToId_(node.parentId as string);
    const parent = pathToParent[pathToParent.length - 1]!;
    if (!parent.children) {
      // Newly created folders in this session may not have an array of
      // children yet, so create an empty one.
      parent.children = [];
    }
    parent.children!.splice(node.index!, 0, node);
    if (this.visibleParent_(parent)) {
      this.shownBookmarks_.unshift(node);
      this.sortBookmarks_(this.shownBookmarks_);
      this.shownBookmarks_ = this.shownBookmarks_.slice();
      getAnnouncerInstance().announce(
          loadTimeData.getStringF('bookmarkCreated', getBookmarkName(node)));
    }
    this.findBookmarkDescriptions_(parent, false);
    this.findBookmarkDescriptions_(node, false);
  }

  private onMoved_(movedInfo: chrome.bookmarks.MoveInfo) {
    // Get old path and remove node from oldParent at oldIndex.
    const oldParentPath = this.findPathToId_(movedInfo.oldParentId);
    const oldParent = oldParentPath[oldParentPath.length - 1]!;
    const movedNode = oldParent!.children![movedInfo.oldIndex]!;
    Object.assign(
        movedNode, {index: movedInfo.index, parentId: movedInfo.parentId});
    oldParent.children!.splice(movedInfo.oldIndex, 1);

    // Get new parent's path and add the node to the new parent at index.
    const newParentPath = this.findPathToId_(movedInfo.parentId);
    const newParent = newParentPath[newParentPath.length - 1]!;
    if (!newParent.children) {
      newParent.children = [];
    }
    newParent.children!.splice(movedInfo.index, 0, movedNode);

    const shouldUpdateUIAdded = this.visibleParent_(newParent);
    const shouldUpdateUIRemoved = this.visibleParent_(oldParent);
    const shouldUpdateUIReordered =
        shouldUpdateUIAdded && shouldUpdateUIRemoved;

    if (shouldUpdateUIReordered) {
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkReordered', getBookmarkName(movedNode)));
    } else if (shouldUpdateUIAdded) {
      this.shownBookmarks_.unshift(movedNode);
      this.sortBookmarks_(this.shownBookmarks_);
      this.shownBookmarks_ = this.shownBookmarks_.slice();
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkMoved', getBookmarkName(movedNode),
          getBookmarkName(newParent)));
    } else if (shouldUpdateUIRemoved) {
      this.splice('shownBookmarks_', this.visibleIndex_(movedNode.id), 1);
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkMoved', getBookmarkName(movedNode),
          getBookmarkName(newParent)));
    }

    if (movedInfo.oldParentId !== movedInfo.parentId) {
      this.findBookmarkDescriptions_(oldParent, false);
      this.findBookmarkDescriptions_(newParent, false);
    }
  }

  private onRemoved_(id: string) {
    const oldPath = this.findPathToId_(id);
    const removedNode = oldPath.pop()!;
    const oldParent = oldPath[oldPath.length - 1]!;
    oldParent.children!.splice(oldParent.children!.indexOf(removedNode), 1);
    this.productInfos_.delete(id);
    const visibleIndex = this.visibleIndex_(id);
    if (visibleIndex > -1) {
      this.splice('shownBookmarks_', visibleIndex, 1);
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkDeleted', getBookmarkName(removedNode)));
    }
    this.findBookmarkDescriptions_(oldParent, false);
  }

  /**
   * Finds the node within all bookmarks and returns the path to the node in
   * the tree.
   */
  private findPathToId_(id: string): chrome.bookmarks.BookmarkTreeNode[] {
    const path: chrome.bookmarks.BookmarkTreeNode[] = [];

    function findPathByIdInternal(
        id: string, node: chrome.bookmarks.BookmarkTreeNode) {
      if (node.id === id) {
        path.push(node);
        return true;
      }

      if (!node.children) {
        return false;
      }

      path.push(node);
      const foundInChildren =
          node.children.some(child => findPathByIdInternal(id, child));
      if (!foundInChildren) {
        path.pop();
      }

      return foundInChildren;
    }

    this.folders_.some(bookmark => findPathByIdInternal(id, bookmark));
    return path;
  }

  private getActiveFolder_(): chrome.bookmarks.BookmarkTreeNode|undefined {
    if (this.activeFolderPath_.length) {
      return this.activeFolderPath_[this.activeFolderPath_.length - 1];
    }
    return undefined;
  }

  /**
   * Reduces an array of nodes to a string to notify Polymer of changes to the
   * nested array.
   */
  private getPathString_(path: chrome.bookmarks.BookmarkTreeNode[]): string {
    return path.reduce((reducedString, pathItem, index) => {
      if (index === 0) {
        return `folders_.${this.folders_.indexOf(pathItem)}`;
      }

      const parent = path[index - 1];
      return `${reducedString}.children.${parent!.children!.indexOf(pathItem)}`;
    }, '');
  }

  /**
   * Assigns a text description for the given bookmark, to be displayed
   * following the bookmark title. Also assigns a description to all
   * descendants if recurse is true.
   */
  private findBookmarkDescriptions_(
      bookmark: chrome.bookmarks.BookmarkTreeNode, recurse: boolean) {
    if (bookmark.children) {
      PluralStringProxyImpl.getInstance()
          .getPluralString('bookmarkFolderChildCount', bookmark.children.length)
          .then(pluralString => {
            this.set(`compactDescriptions_.${bookmark.id}`, pluralString);
          });
    }
    if (bookmark.url) {
      const url = new URL(bookmark.url);
      // Show chrome:// if it's a chrome internal url
      if (url.protocol === 'chrome:') {
        this.set(
            `expandedDescriptions_.${bookmark.id}`, 'chrome://' + url.hostname);
      } else {
        this.set(`expandedDescriptions_.${bookmark.id}`, url.hostname);
      }
    }
    if (recurse && bookmark.children) {
      bookmark.children.forEach(
          child => this.findBookmarkDescriptions_(child, recurse));
    }
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
      return this.get(`expandedDescriptions_.${bookmark.id}`);
    }
  }

  private getFolderLabel_(): string {
    const activeFolder = this.getActiveFolder_();
    if (activeFolder) {
      return activeFolder!.title;
    } else {
      return loadTimeData.getString('allBookmarks');
    }
  }

  private getSortLabel_(): string {
    return this.sortTypes_[this.activeSortIndex_]!;
  }

  private getProductInfos_(): BookmarkProductInfo[] {
    return Array.from(this.productInfos_.values());
  }

  private isPriceTracked_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      boolean {
    return this.productInfos_.has(bookmark.id);
  }

  /**
   * Update the list of bookmarks and folders displayed to the user.
   */
  private updateShownBookmarks_() {
    let shownBookmarks;
    const activeFolder = this.getActiveFolder_();
    if (activeFolder) {
      shownBookmarks = activeFolder.children!.slice();
    } else {
      let topLevelBookmarks: chrome.bookmarks.BookmarkTreeNode[] = [];
      this.folders_.forEach(
          folder => topLevelBookmarks =
              topLevelBookmarks.concat(folder.children!));
      shownBookmarks = topLevelBookmarks;
    }
    shownBookmarks = shownBookmarks.filter(
        (b: chrome.bookmarks.BookmarkTreeNode) =>
            this.nodeMatchesContentFilters_(b));
    const sortChangedPosition = this.sortBookmarks_(shownBookmarks);
    if (sortChangedPosition) {
      this.shownBookmarks_ = shownBookmarks.slice();
    } else {
      this.shownBookmarks_ = shownBookmarks;
    }
  }

  private canAddCurrentUrl_(): boolean {
    const activeFolder =
        this.activeFolderPath_[this.activeFolderPath_.length - 1];
    let unfilteredShownBookmarks: chrome.bookmarks.BookmarkTreeNode[] = [];
    if (activeFolder) {
      unfilteredShownBookmarks = activeFolder.children!;
    } else {
      this.folders_.forEach(
          folder => unfilteredShownBookmarks =
              unfilteredShownBookmarks.concat(folder.children!));
    }
    return unfilteredShownBookmarks.findIndex(
               b => b.url === this.currentUrl_) === -1;
  }

  private nodeMatchesContentFilters_(
      bookmark: chrome.bookmarks.BookmarkTreeNode): boolean {
    // Price tracking label
    if (this.labels_[0]!.active && !this.isPriceTracked_(bookmark)) {
      return false;
    } else if (
        this.searchQuery_ &&
        !(bookmark.title &&
          bookmark.title.toLocaleLowerCase().includes(this.searchQuery_!)) &&
        !(bookmark.url &&
          bookmark.url.toLocaleLowerCase().includes(this.searchQuery_!))) {
      return false;
    }
    return true;
  }

  /**
   * Apply the current active sort type to the given bookmarks list. Returns
   * true if any elements in the list changed position.
   */
  private sortBookmarks_(bookmarks: chrome.bookmarks.BookmarkTreeNode[]):
      boolean {
    const activeSortIndex = this.activeSortIndex_;
    let changedPosition = false;
    bookmarks.sort(function(
        a: chrome.bookmarks.BookmarkTreeNode,
        b: chrome.bookmarks.BookmarkTreeNode) {
      // Always sort by folders first
      if (a.children && !b.children) {
        return -1;
      } else if (!a.children && b.children) {
        changedPosition = true;
        return 1;
      } else {
        let toReturn;
        if (activeSortIndex === 0) {
          // Newest first
          toReturn = b.dateAdded! - a.dateAdded!;
        } else if (activeSortIndex === 1) {
          // Oldest first
          toReturn = a.dateAdded! - b.dateAdded!;
        } else if (activeSortIndex === 2) {
          // Alphabetical
          toReturn = a.title!.localeCompare(b.title);
        } else {
          // Reverse alphabetical
          toReturn = b.title!.localeCompare(a.title);
        }
        if (toReturn > 0) {
          changedPosition = true;
        }
        return toReturn;
      }
    });
    return changedPosition;
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
          event.detail.event, event.detail.bookmark,
          this.activeFolderPath_.length);
    } else {
      this.$.contextMenu.get().showAtPosition(
          event.detail.event, event.detail.bookmark,
          this.activeFolderPath_.length);
    }
  }

  private onShowSortMenuClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.$.sortMenu.showAt(event.target as HTMLElement);
  }

  private onAddNewFolderClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    // TODO: Implement add new folder functionality
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
          this.selectedBookmarks_ = [];
          this.editing_ = false;
        });
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
    const newParent = this.getActiveFolder_() ||
        this.folders_.find(
            (folder: chrome.bookmarks.BookmarkTreeNode) =>
                folder.id === loadTimeData.getString('otherBookmarksId'));
    this.bookmarksApi_.bookmarkCurrentTabInFolder(newParent!.id);
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
    const bookmarkProductInfo = this.productInfos_.get(bookmark.id);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.previousPrice.length > 0;
    }
    return false;
  }

  private getCurrentPrice_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      string {
    const bookmarkProductInfo = this.productInfos_.get(bookmark.id);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.currentPrice;
    } else {
      return '';
    }
  }

  private getPreviousPrice_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      string {
    const bookmarkProductInfo = this.productInfos_.get(bookmark.id);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.previousPrice;
    } else {
      return '';
    }
  }

  private onBookmarkPriceTracked() {
    // Here we only control the visibility of ShoppingListElement. The same
    // signal will also be handled in ShoppingListElement to update shopping
    // list.
    if (this.productInfos_.size > 0) {
      return;
    }
    this.showPriceTracking_ = true;
    chrome.metricsPrivate.recordUserAction(
        'Commerce.PriceTracking.SidePanel.TrackedProductsShown');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'power-bookmarks-list': PowerBookmarksListElement;
  }
}

customElements.define(PowerBookmarksListElement.is, PowerBookmarksListElement);
