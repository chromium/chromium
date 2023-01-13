// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';

export interface BookmarkManagerApiProxy {
  onDragEnter:
      ChromeEvent<(p1: chrome.bookmarkManagerPrivate.DragData) => void>;

  drop(parentId: string, index?: number): Promise<void>;
  startDrag(
      idList: string[], dragNodeIndex: number, isFromTouch: boolean, x: number,
      y: number): void;
  removeTrees(idList: string[]): Promise<void>;
}

export class BookmarkManagerApiProxyImpl implements BookmarkManagerApiProxy {
  onDragEnter = chrome.bookmarkManagerPrivate.onDragEnter;

  drop(parentId: string, index?: number) {
    return chrome.bookmarkManagerPrivate.drop(parentId, index);
  }

  startDrag(
      idList: string[], dragNodeIndex: number, isFromTouch: boolean, x: number,
      y: number) {
    return chrome.bookmarkManagerPrivate.startDrag(
        idList, dragNodeIndex, isFromTouch, x, y);
  }

  removeTrees(idList: string[]) {
    return chrome.bookmarkManagerPrivate.removeTrees(idList);
  }

  static getInstance(): BookmarkManagerApiProxy {
    return instance || (instance = new BookmarkManagerApiProxyImpl());
  }

  static setInstance(obj: BookmarkManagerApiProxy) {
    instance = obj;
  }
}

let instance: BookmarkManagerApiProxy|null = null;
