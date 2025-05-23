// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';
import type {ClickModifiers} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';

import type {ActionSource, SortOrder, ViewType} from './bookmarks.mojom-webui.js';
import {BookmarksPageCallbackRouter, BookmarksPageHandlerFactory, BookmarksPageHandlerRemote} from './bookmarks.mojom-webui.js';
import type {BookmarksTreeNode} from './bookmarks.mojom-webui.js';

let instance: BookmarksApiProxy|null = null;

export interface BookmarksApiProxy {
  callbackRouter: {[key: string]: ChromeEvent<Function>};
  pageCallbackRouter: BookmarksPageCallbackRouter;

  // Side Panel Activation and basic bookmark information retrieval.
  showUi(): void;
  getAllBookmarks(): Promise<{nodes: BookmarksTreeNode[]}>;
  getActiveUrl(): Promise<string|undefined>;
  isActiveTabInSplit(): Promise<boolean>;

  // Side Panel display choices.
  setSortOrder(sortOrder: SortOrder): void;
  setViewType(viewType: ViewType): void;

  // Side Panel operations.
  bookmarkCurrentTabInFolder(folderId: string): void;
  createFolder(parentId: string, title: string): Promise<{newFolderId: string}>;
  deleteBookmarks(ids: string[]): Promise<void>;
  dropBookmarks(parentId: string): Promise<void>;
  editBookmarks(
      ids: string[], newTitle: string|undefined, newUrl: string|undefined,
      newParentId: string|undefined): void;
  undo(): void;
  renameBookmark(id: string, title: string): void;
  openBookmark(
      id: string, depth: number, clickModifiers: ClickModifiers,
      source: ActionSource): void;

  // Context menu.
  showContextMenu(id: string, x: number, y: number, source: ActionSource): void;
  contextMenuOpenBookmarkInNewTab(ids: string[], source: ActionSource): void;
  contextMenuOpenBookmarkInNewWindow(ids: string[], source: ActionSource): void;
  contextMenuOpenBookmarkInIncognitoWindow(ids: string[], source: ActionSource):
      void;
  contextMenuOpenBookmarkInNewTabGroup(ids: string[], source: ActionSource):
      void;
  contextMenuOpenBookmarkInSplitView(ids: string[], source: ActionSource): void;
  contextMenuEdit(ids: string[], source: ActionSource): void;
  contextMenuMove(ids: string[], source: ActionSource): void;
  contextMenuAddToBookmarksBar(id: string, source: ActionSource): void;
  contextMenuRemoveFromBookmarksBar(id: string, source: ActionSource): void;
  contextMenuDelete(ids: string[], source: ActionSource): void;
}

export class BookmarksApiProxyImpl implements BookmarksApiProxy {
  callbackRouter: {[key: string]: ChromeEvent<Function>};

  pageCallbackRouter: BookmarksPageCallbackRouter;
  handler: BookmarksPageHandlerRemote;

  constructor() {
    this.callbackRouter = {
      onTabActivated: chrome.tabs.onActivated,
      onTabUpdated: chrome.tabs.onUpdated,
    };

    this.pageCallbackRouter = new BookmarksPageCallbackRouter();
    this.handler = new BookmarksPageHandlerRemote();

    const factory = BookmarksPageHandlerFactory.getRemote();
    factory.createBookmarksPageHandler(
        this.pageCallbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  bookmarkCurrentTabInFolder(folderId: string) {
    this.handler.bookmarkCurrentTabInFolder(folderId);
  }

  contextMenuOpenBookmarkInNewTab(ids: string[], source: ActionSource) {
    this.handler.executeOpenInNewTabCommand(ids, source);
  }

  contextMenuOpenBookmarkInNewWindow(ids: string[], source: ActionSource) {
    this.handler.executeOpenInNewWindowCommand(ids, source);
  }

  contextMenuOpenBookmarkInIncognitoWindow(
      ids: string[], source: ActionSource) {
    this.handler.executeOpenInIncognitoWindowCommand(ids, source);
  }

  contextMenuOpenBookmarkInNewTabGroup(ids: string[], source: ActionSource) {
    this.handler.executeOpenInNewTabGroupCommand(ids, source);
  }

  contextMenuOpenBookmarkInSplitView(ids: string[], source: ActionSource) {
    this.handler.executeOpenInSplitViewCommand(
        ids.map(id => BigInt(id)), source);
  }

  contextMenuEdit(ids: string[], source: ActionSource) {
    this.handler.executeEditCommand(ids.map(id => BigInt(id)), source);
  }

  contextMenuMove(ids: string[], source: ActionSource) {
    this.handler.executeMoveCommand(ids.map(id => BigInt(id)), source);
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

  createFolder(parentId: string, title: string) {
    return this.handler.createFolder(parentId, title);
  }

  deleteBookmarks(ids: string[]) {
    return this.handler.removeBookmarks(ids.map(id => BigInt(id)));
  }

  dropBookmarks(parentId: string) {
    return this.handler.dropBookmarks(parentId);
  }

  editBookmarks(
      ids: string[], newTitle: string|undefined, newUrl: string|undefined,
      newParentId: string|undefined) {
    // Current use cases do not expect one of newTitle and newUrl to be
    // provided without the other.
    if (newTitle !== undefined && newUrl !== undefined) {
      ids.forEach(id => {
        // TODO(crbug.com/408181043): Keeping this extensions call as an
        // exception since this dialog will not be used anymore after Butter for
        // Bookmarks will be launched.
        chrome.bookmarks.update(id, {title: newTitle, url: newUrl});
      });
    }
    if (newParentId) {
      ids.forEach(id => {
        this.handler.moveBookmark(BigInt(id), newParentId);
      });
    }
  }

  getActiveUrl() {
    return chrome.tabs.query({active: true, currentWindow: true}).then(tabs => {
      if (tabs[0]) {
        return tabs[0].url;
      }
      return undefined;
    });
  }

  // TODO(crbug.com/406794014): Use the extensions API for this once
  // implemented.
  isActiveTabInSplit() {
    return chrome.bookmarkManagerPrivate.isActiveTabInSplit();
  }

  openBookmark(
      id: string, depth: number, clickModifiers: ClickModifiers,
      source: ActionSource) {
    this.handler.openBookmark(BigInt(id), depth, clickModifiers, source);
  }

  renameBookmark(id: string, title: string) {
    this.handler.renameBookmark(BigInt(id), title);
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
    this.handler.undo();
  }

  // Asynchronously gets the list of non empty permanent bookmark nodes.
  getAllBookmarks(): Promise<{nodes: BookmarksTreeNode[]}> {
    return this.handler.getAllBookmarks();
  }

  static getInstance(): BookmarksApiProxy {
    return instance || (instance = new BookmarksApiProxyImpl());
  }

  static setInstance(obj: BookmarksApiProxy) {
    instance = obj;
  }
}
