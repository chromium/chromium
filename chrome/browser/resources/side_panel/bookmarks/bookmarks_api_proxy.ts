// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';
import type {ClickModifiers} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';

import type {ActionSource, SortOrder, ViewType} from './bookmarks.mojom-webui.js';
import {BookmarksPageHandlerFactory, BookmarksPageHandlerRemote} from './bookmarks.mojom-webui.js';

let instance: BookmarksApiProxy|null = null;

export interface BookmarksApiProxy {
  callbackRouter: {[key: string]: ChromeEvent<Function>};
  bookmarkCurrentTabInFolder(folderId: string): void;
  cutBookmark(id: string): void;
  contextMenuOpenBookmarkInNewTab(ids: string[], source: ActionSource): void;
  contextMenuOpenBookmarkInNewWindow(ids: string[], source: ActionSource): void;
  contextMenuOpenBookmarkInIncognitoWindow(ids: string[], source: ActionSource):
      void;
  contextMenuOpenBookmarkInNewTabGroup(ids: string[], source: ActionSource):
      void;
  contextMenuAddToBookmarksBar(id: string, source: ActionSource): void;
  contextMenuRemoveFromBookmarksBar(id: string, source: ActionSource): void;
  contextMenuDelete(ids: string[], source: ActionSource): void;
  copyBookmark(id: string): Promise<void>;
  createFolder(parentId: string, title: string):
      Promise<chrome.bookmarks.BookmarkTreeNode>;
  editBookmarks(
      ids: string[], newTitle: string|undefined, newUrl: string|undefined,
      newParentId: string|undefined): void;
  deleteBookmarks(ids: string[]): Promise<void>;
  getActiveUrl(): Promise<string|undefined>;
  getFolders(): Promise<chrome.bookmarks.BookmarkTreeNode[]>;
  openBookmark(
      id: string, depth: number, clickModifiers: ClickModifiers,
      source: ActionSource): void;
  pasteToBookmark(parentId: string, destinationId?: string): Promise<void>;
  renameBookmark(id: string, title: string): void;
  setSortOrder(sortOrder: SortOrder): void;
  setViewType(viewType: ViewType): void;
  showContextMenu(id: string, x: number, y: number, source: ActionSource): void;
  showUi(): void;
  undo(): void;
}

export class BookmarksApiProxyImpl implements BookmarksApiProxy {
  callbackRouter: {[key: string]: ChromeEvent<Function>};
  handler: BookmarksPageHandlerRemote;

  constructor() {
    this.callbackRouter = {
      onChanged: chrome.bookmarks.onChanged,
      onChildrenReordered: chrome.bookmarks.onChildrenReordered,
      onCreated: chrome.bookmarks.onCreated,
      onMoved: chrome.bookmarks.onMoved,
      onRemoved: chrome.bookmarks.onRemoved,
      onTabActivated: chrome.tabs.onActivated,
      onTabUpdated: chrome.tabs.onUpdated,
    };

    this.handler = new BookmarksPageHandlerRemote();

    const factory = BookmarksPageHandlerFactory.getRemote();
    factory.createBookmarksPageHandler(
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  bookmarkCurrentTabInFolder(folderId: string) {
    this.handler.bookmarkCurrentTabInFolder(BigInt(folderId));
  }

  cutBookmark(id: string) {
    chrome.bookmarkManagerPrivate.cut([id]);
  }

  contextMenuOpenBookmarkInNewTab(ids: string[], source: ActionSource) {
    this.handler.executeOpenInNewTabCommand(ids.map(id => BigInt(id)), source);
  }

  contextMenuOpenBookmarkInNewWindow(ids: string[], source: ActionSource) {
    this.handler.executeOpenInNewWindowCommand(
        ids.map(id => BigInt(id)), source);
  }

  contextMenuOpenBookmarkInIncognitoWindow(
      ids: string[], source: ActionSource) {
    this.handler.executeOpenInIncognitoWindowCommand(
        ids.map(id => BigInt(id)), source);
  }

  contextMenuOpenBookmarkInNewTabGroup(ids: string[], source: ActionSource) {
    this.handler.executeOpenInNewTabGroupCommand(
        ids.map(id => BigInt(id)), source);
  }

  contextMenuAddToBookmarksBar(id: string, source: ActionSource) {
    this.handler.executeAddToBookmarksBarCommand(BigInt(id), source);
  }

  contextMenuRemoveFromBookmarksBar(id: string, source: ActionSource) {
    this.handler.executeRemoveFromBookmarksBarCommand(BigInt(id), source);
  }

  contextMenuDelete(ids: string[], source: ActionSource) {
    this.handler.executeDeleteCommand(ids.map(id => BigInt(id)), source);
  }

  copyBookmark(id: string) {
    return chrome.bookmarkManagerPrivate.copy([id]);
  }

  createFolder(parentId: string, title: string) {
    return chrome.bookmarks.create(
        {parentId: parentId, title: title, index: 0});
  }

  editBookmarks(
      ids: string[], newTitle: string|undefined, newUrl: string|undefined,
      newParentId: string|undefined) {
    // Current use cases do not expect one of newTitle and newUrl to be
    // provided without the other.
    if (newTitle !== undefined && newUrl !== undefined) {
      ids.forEach(id => {
        chrome.bookmarks.update(id, {title: newTitle, url: newUrl});
      });
    }
    if (newParentId) {
      ids.forEach(id => {
        chrome.bookmarks.move(id, {parentId: newParentId});
      });
    }
  }

  deleteBookmarks(ids: string[]) {
    return chrome.bookmarkManagerPrivate.removeTrees(ids);
  }

  getActiveUrl() {
    return chrome.tabs.query({active: true, currentWindow: true}).then(tabs => {
      if (tabs[0]) {
        return tabs[0].url;
      }
      return undefined;
    });
  }

  getFolders() {
    return chrome.bookmarks.getTree().then(results => {
      if (results[0] && results[0].children) {
        return results[0].children;
      }
      return [];
    });
  }

  openBookmark(
      id: string, depth: number, clickModifiers: ClickModifiers,
      source: ActionSource) {
    this.handler.openBookmark(BigInt(id), depth, clickModifiers, source);
  }

  pasteToBookmark(parentId: string, destinationId?: string) {
    const destination = destinationId ? [destinationId] : [];
    return chrome.bookmarkManagerPrivate.paste(parentId, destination);
  }

  renameBookmark(id: string, title: string) {
    chrome.bookmarks.update(id, {title: title});
  }

  setSortOrder(sortOrder: SortOrder) {
    this.handler.setSortOrder(sortOrder);
  }

  setViewType(viewType: ViewType) {
    this.handler.setViewType(viewType);
  }

  showContextMenu(id: string, x: number, y: number, source: ActionSource) {
    this.handler.showContextMenu(id, {x, y}, source);
  }

  showUi() {
    this.handler.showUI();
  }

  undo() {
    chrome.bookmarkManagerPrivate.undo();
  }

  static getInstance(): BookmarksApiProxy {
    return instance || (instance = new BookmarksApiProxyImpl());
  }

  static setInstance(obj: BookmarksApiProxy) {
    instance = obj;
  }
}
