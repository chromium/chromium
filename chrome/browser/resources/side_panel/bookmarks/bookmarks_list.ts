// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './commerce/shopping_list.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {listenOnce} from 'chrome://resources/js/util_ts.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BookmarkFolderElement, FOLDER_OPEN_CHANGED_EVENT, getBookmarkFromElement, isBookmarkFolderElement} from './bookmark_folder.js';
import {BookmarksApiProxy, BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';
import {BookmarksDragManager} from './bookmarks_drag_manager.js';
import {getTemplate} from './bookmarks_list.html.js';
import {BookmarkProductInfo} from './commerce/shopping_list.mojom-webui.js';
import {ShoppingListApiProxy, ShoppingListApiProxyImpl} from './commerce/shopping_list_api_proxy.js';

// Key for localStorage object that refers to all the open folders.
export const LOCAL_STORAGE_OPEN_FOLDERS_KEY = 'openFolders';

function getBookmarkName(bookmark: chrome.bookmarks.BookmarkTreeNode): string {
  return bookmark.title || bookmark.url || '';
}

export interface BookmarksListElement {
  $: {
    bookmarksContainer: HTMLElement,
  };
}

export class BookmarksListElement extends PolymerElement {
  static get is() {
    return 'bookmarks-list';
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

      hoverVisible: {
        reflectToAttribute: true,
        value: false,
      },

      openFolders_: {
        type: Array,
        value: () => [],
      },
    };
  }

  private bookmarksApi_: BookmarksApiProxy =
      BookmarksApiProxyImpl.getInstance();
  private shoppingListApi_: ShoppingListApiProxy =
      ShoppingListApiProxyImpl.getInstance();
  private bookmarksDragManager_: BookmarksDragManager =
      new BookmarksDragManager(this);
  private focusOutlineManager_: FocusOutlineManager;
  private listeners_ = new Map<string, Function>();
  private folders_: chrome.bookmarks.BookmarkTreeNode[];
  private productInfos_: BookmarkProductInfo[];
  hoverVisible: boolean;
  private openFolders_: string[];
  private shoppingListenerIds_: number[] = [];

  override ready() {
    super.ready();
    this.addEventListener(
        FOLDER_OPEN_CHANGED_EVENT,
        e => this.onFolderOpenChanged_(
            e as CustomEvent<{id: string, open: boolean}>));
    this.addEventListener('keydown', e => this.onKeydown_(e));
    this.addEventListener('pointermove', () => this.hoverVisible = true);
    this.addEventListener('pointerleave', () => this.hoverVisible = false);
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setAttribute('role', 'tree');
    if (loadTimeData.getBoolean('unifiedSidePanel')) {
      listenOnce(this.$.bookmarksContainer, 'dom-change', () => {
        setTimeout(() => this.bookmarksApi_.showUi(), 0);
      });
    }
    this.focusOutlineManager_ = FocusOutlineManager.forDocument(document);
    this.bookmarksApi_.getFolders().then(folders => {
      this.folders_ = folders;

      this.addListener_(
          'onChildrenReordered',
          (id: string, reorderedInfo: chrome.bookmarks.ReorderInfo) =>
              this.onChildrenReordered_(id, reorderedInfo));
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

      try {
        const openFolders = window.localStorage[LOCAL_STORAGE_OPEN_FOLDERS_KEY];
        this.openFolders_ = JSON.parse(openFolders);
      } catch (error) {
        this.openFolders_ = [this.folders_[0]!.id];
        window.localStorage[LOCAL_STORAGE_OPEN_FOLDERS_KEY] =
            JSON.stringify(this.openFolders_);
      }

      this.bookmarksDragManager_.startObserving();
    });

    this.shoppingListApi_.getAllPriceTrackedBookmarkProductInfo().then(res => {
      this.productInfos_ = res.productInfos;
      if (this.productInfos_.length > 0) {
        chrome.metricsPrivate.recordUserAction(
            'Commerce.PriceTracking.SidePanel.TrackedProductsShown');
      }
    });
    const callbackRouter = this.shoppingListApi_.getCallbackRouter();
    this.shoppingListenerIds_.push(
        callbackRouter.priceTrackedForBookmark.addListener(
            (product: BookmarkProductInfo) =>
                this.onBookmarkPriceTracked(product)),
    );
  }

  override disconnectedCallback() {
    for (const [eventName, callback] of this.listeners_.entries()) {
      this.bookmarksApi_.callbackRouter[eventName]!.removeListener(callback);
    }
    this.bookmarksDragManager_.stopObserving();
    this.shoppingListenerIds_.forEach(
        id => this.shoppingListApi_.getCallbackRouter().removeListener(id));
  }

  /** BookmarksDragDelegate */
  getAscendants(bookmarkId: string): string[] {
    const path = this.findPathToId_(bookmarkId);
    return path.map(bookmark => bookmark.id);
  }

  /** BookmarksDragDelegate */
  getIndex(bookmark: chrome.bookmarks.BookmarkTreeNode): number {
    const path = this.findPathToId_(bookmark.id);
    const parent = path[path.length - 2];
    if (!parent || !parent.children) {
      return -1;
    }
    return parent.children.findIndex((child) => child.id === bookmark.id);
  }

  /** BookmarksDragDelegate */
  isFolderOpen(bookmark: chrome.bookmarks.BookmarkTreeNode): boolean {
    return this.openFolders_.some(id => bookmark.id === id);
  }

  /** BookmarksDragDelegate */
  onFinishDrop(bookmarks: chrome.bookmarks.BookmarkTreeNode[]): void {
    if (bookmarks.length === 0 || bookmarks[0]!.id === undefined) {
      return;
    }

    this.focusBookmark_(bookmarks[0]!.id);
    this.hoverVisible = false;

    // Show the focus state immediately after dropping a bookmark to indicate
    // where the bookmark was moved to, and remove the state immediately after
    // the next mouse event.
    this.focusOutlineManager_.visible = true;
    document.addEventListener('mousedown', () => {
      this.focusOutlineManager_.visible = false;
    }, {once: true});
  }

  /** BookmarksDragDelegate */
  openFolder(folderId: string) {
    this.changeFolderOpenStatus_(folderId, true);
  }

  private addListener_(eventName: string, callback: Function): void {
    this.bookmarksApi_.callbackRouter[eventName]!.addListener(callback);
    this.listeners_.set(eventName, callback);
  }

  /**
   * Finds the node within the nested array of folders and returns the path to
   * the node in the tree.
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

    this.folders_.some(folder => findPathByIdInternal(id, folder));
    return path;
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

  private onChanged_(id: string, changedInfo: chrome.bookmarks.ChangeInfo) {
    const path = this.findPathToId_(id);
    Object.assign(path[path.length - 1], changedInfo);

    const pathString = this.getPathString_(path);
    Object.keys(changedInfo)
        .forEach(key => this.notifyPath(`${pathString}.${key}`));
  }

  private onChildrenReordered_(
      id: string, reorderedInfo: chrome.bookmarks.ReorderInfo) {
    const path = this.findPathToId_(id);
    const parent = path[path.length - 1];
    const childById = parent!.children!.reduce((map, node) => {
      map.set(node.id, node);
      return map;
    }, new Map());
    parent!.children = reorderedInfo.childIds.map(id => childById.get(id));
    const pathString = this.getPathString_(path);
    this.notifyPath(`${pathString}.children`);
  }

  private onCreated_(node: chrome.bookmarks.BookmarkTreeNode) {
    const pathToParent = this.findPathToId_(node.parentId as string);
    const pathToParentString = this.getPathString_(pathToParent);
    const parent = pathToParent[pathToParent.length - 1];
    if (parent && !parent.children) {
      // Newly created folders in this session may not have an array of
      // children yet, so create an empty one.
      parent.children = [];
    }
    this.splice(`${pathToParentString}.children`, node.index!, 0, node);
    afterNextRender(this, () => {
      this.focusBookmark_(node.id);
      getAnnouncerInstance().announce(
          loadTimeData.getStringF('bookmarkCreated', getBookmarkName(node)));
    });
  }

  private changeFolderOpenStatus_(id: string, open: boolean) {
    const alreadyOpenIndex = this.openFolders_.indexOf(id);
    if (open && alreadyOpenIndex === -1) {
      this.openFolders_.push(id);
    } else if (!open) {
      this.openFolders_.splice(alreadyOpenIndex, 1);
    }

    // Assign to a new array so that listeners are triggered.
    this.openFolders_ = [...this.openFolders_];
    window.localStorage[LOCAL_STORAGE_OPEN_FOLDERS_KEY] =
        JSON.stringify(this.openFolders_);
  }

  private onFolderOpenChanged_(event: CustomEvent) {
    const {id, open} = event.detail;
    this.changeFolderOpenStatus_(id, open);
  }

  private onKeydown_(event: KeyboardEvent) {
    if (['ArrowDown', 'ArrowUp'].includes(event.key)) {
      this.handleArrowKeyNavigation_(event);
      return;
    }

    if (!event.ctrlKey && !event.metaKey) {
      return;
    }
    event.preventDefault();
    const eventTarget = event.composedPath()[0] as HTMLElement;
    const bookmarkData = getBookmarkFromElement(eventTarget);
    if (!bookmarkData || !this.focusOutlineManager_.visible) {
      return;
    }

    if (event.key === 'x') {
      this.bookmarksApi_.cutBookmark(bookmarkData.id);
    } else if (event.key === 'c') {
      this.bookmarksApi_.copyBookmark(bookmarkData.id);
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkCopied', getBookmarkName(bookmarkData)));
    } else if (event.key === 'v') {
      if (isBookmarkFolderElement(eventTarget)) {
        this.bookmarksApi_.pasteToBookmark(bookmarkData.id);
      } else {
        this.bookmarksApi_.pasteToBookmark(
            bookmarkData.parentId!, bookmarkData.id);
      }
    }
  }

  private handleArrowKeyNavigation_(event: KeyboardEvent) {
    if (!(this.shadowRoot!.activeElement instanceof BookmarkFolderElement)) {
      // If the key event did not happen within a BookmarkFolderElement, do
      // not do anything.
      return;
    }

    // Prevent arrow keys from causing scroll.
    event.preventDefault();

    const allFolderElements: BookmarkFolderElement[] =
        Array.from(this.shadowRoot!.querySelectorAll('bookmark-folder'));

    const delta = event.key === 'ArrowUp' ? -1 : 1;
    let currentIndex =
        allFolderElements.indexOf(this.shadowRoot!.activeElement);
    let focusHasMoved = false;
    while (!focusHasMoved) {
      focusHasMoved = allFolderElements[currentIndex]!.moveFocus(delta);
      currentIndex = (currentIndex + delta + allFolderElements.length) %
          allFolderElements.length;
    }
  }

  private onMoved_(movedInfo: chrome.bookmarks.MoveInfo) {
    // Get old path and remove node from oldParent at oldIndex.
    const oldParentPath = this.findPathToId_(movedInfo.oldParentId);
    const oldParentPathString = this.getPathString_(oldParentPath);
    const oldParent = oldParentPath[oldParentPath.length - 1];
    const movedNode = oldParent!.children![movedInfo.oldIndex]!;
    Object.assign(
        movedNode, {index: movedInfo.index, parentId: movedInfo.parentId});
    this.splice(`${oldParentPathString}.children`, movedInfo.oldIndex, 1);

    // Get new parent's path and add the node to the new parent at index.
    const newParentPath = this.findPathToId_(movedInfo.parentId);
    const newParentPathString = this.getPathString_(newParentPath);
    const newParent = newParentPath[newParentPath.length - 1];
    if (newParent && !newParent.children) {
      newParent.children = [];
    }
    this.splice(
        `${newParentPathString}.children`, movedInfo.index, 0, movedNode);

    if (movedInfo.oldParentId === movedInfo.parentId) {
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkReordered', getBookmarkName(movedNode)));
    } else {
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkMoved', getBookmarkName(movedNode),
          getBookmarkName(newParent!)));
    }
  }

  private onRemoved_(id: string) {
    const oldPath = this.findPathToId_(id);
    const removedNode = oldPath.pop()!;
    const oldParent = oldPath[oldPath.length - 1]!;
    const oldParentPathString = this.getPathString_(oldPath);
    this.splice(
        `${oldParentPathString}.children`,
        oldParent.children!.indexOf(removedNode), 1);

    getAnnouncerInstance().announce(loadTimeData.getStringF(
        'bookmarkDeleted', getBookmarkName(removedNode)));
    this.productInfos_ =
        this.productInfos_.filter(item => item.bookmarkId !== BigInt(id));
  }

  private focusBookmark_(id: string) {
    const path = this.findPathToId_(id);
    if (path.length === 0) {
      // Could not find bookmark.
      return;
    }

    const rootBookmark = path.shift();
    const rootBookmarkElement =
        this.shadowRoot!.querySelector(`#bookmark-${rootBookmark!.id}`) as
        BookmarkFolderElement;
    if (!rootBookmarkElement) {
      return;
    }

    const bookmarkElement = rootBookmarkElement.getFocusableElement(path);
    if (bookmarkElement) {
      bookmarkElement.focus();
    }
  }

  private onBookmarkPriceTracked(product: BookmarkProductInfo) {
    // Here we only control the visibility of ShoppingListElement. The same
    // signal will also be handled in ShoppingListElement to update shopping
    // list.
    if (this.productInfos_.length > 0) {
      return;
    }
    this.push('productInfos_', product);
    chrome.metricsPrivate.recordUserAction(
        'Commerce.PriceTracking.SidePanel.TrackedProductsShown');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'bookmarks-list': BookmarksListElement;
  }
}

customElements.define(BookmarksListElement.is, BookmarksListElement);
