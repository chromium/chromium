// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';
import {assert, assertNotReachedCase} from 'chrome://resources/js/assert.js';

import {BookmarksObserverReceiver, BookmarksService} from './bookmarks_api.mojom-webui.js';
import type {BookmarkNodeChanged, BookmarkNodeCreated, BookmarkNodeMoved, BookmarkNodeRemoved, BookmarksEvent, BookmarksObserverInterface} from './bookmarks_api.mojom-webui.js';
import {BookmarksEventFieldTags, whichBookmarksEvent} from './bookmarks_api.mojom-webui.js';
import {Store} from './store.js';
import type {BookmarkNode, NodeMap} from './types.js';
import {normalizeMojoNode, normalizeNodes, toMojoNode} from './util.js';

export type Query = string|{
  query?: string,
  url?: string,
  title?: string,
};

class BookmarkEventForwarder<T extends Function> implements ChromeEvent<T> {
  private listeners_: T[] = [];

  addListener(listener: T) {
    this.listeners_.push(listener);
  }

  removeListener(listener: T) {
    this.listeners_ = this.listeners_.filter(l => l !== listener);
  }

  forward(...args: any[]) {
    this.listeners_.forEach(l => l(...args));
  }
}

// Directs mojo events to our proxy. Once the migration is complete, we should
// collapse this indirection.
class MojoObserver implements BookmarksObserverInterface {
  private proxy_: BookmarksApiProxyImpl;

  constructor(proxy: BookmarksApiProxyImpl) {
    this.proxy_ = proxy;
  }

  onBookmarksEvents(events: BookmarksEvent[]) {
    this.proxy_.handleMojoEvents(events);
  }
}

export interface BookmarksApiProxy {
  getTree(): Promise<NodeMap>;
  update(id: string, changes: {title?: string, url?: string}):
      Promise<BookmarkNode>;
  create(parentId: string, index: number|null, title: string, url?: string):
      Promise<BookmarkNode>;
  delete(idList: string[]): Promise<void>;

  onCreated: ChromeEvent<
      (parentId: string, index: number, node: BookmarkNode) => void>;
  onRemoved: ChromeEvent<(id: string, parentId: string, index: number) => void>;
  onChanged: ChromeEvent<(id: string, node: BookmarkNode) => void>;
  onMoved: ChromeEvent<
      (id: string, oldParentId: string, oldIndex: number, newParentId: string,
       newIndex: number) => void>;
  onChildrenReordered:
      ChromeEvent<(id: string, reorderInfo: {childIds: string[]}) => void>;
}

export class BookmarksApiProxyImpl implements BookmarksApiProxy {
  private receiver_: BookmarksObserverReceiver|null = null;
  private rootNodePromise_: Promise<NodeMap>;

  onCreated = new BookmarkEventForwarder<
      (parentId: string, index: number, node: BookmarkNode) => void>();
  onRemoved = new BookmarkEventForwarder<
      (id: string, parentId: string, index: number) => void>();
  onChanged =
      new BookmarkEventForwarder<(id: string, node: BookmarkNode) => void>();
  onMoved = new BookmarkEventForwarder<
      (id: string, oldParentId: string, oldIndex: number, newParentId: string,
       newIndex: number) => void>();
  onChildrenReordered = new BookmarkEventForwarder<
      (id: string, reorderInfo: {childIds: string[]}) => void>();

  constructor() {
    this.rootNodePromise_ =
        BookmarksService.getRemote().getBookmarks().then(snapshot => {
          // Bind observer.
          const observer = new MojoObserver(this);
          this.receiver_ = new BookmarksObserverReceiver(observer);
          this.receiver_.$.bindHandle(snapshot.stream.handle);

          return normalizeNodes(snapshot.root);
        });
  }

  getTree() {
    return this.rootNodePromise_;
  }

  update(id: string, changes: {title?: string, url?: string}) {
    const store = Store.getInstance();
    const node = store.data.nodes[id];
    if (!node) {
      throw new Error('Node not found in store: ' + id);
    }

    const mojoNode = toMojoNode(node);
    if (mojoNode.url) {
      // Url node type.
      if (changes.title !== undefined) {
        mojoNode.url.title = changes.title;
      }
      if (changes.url !== undefined) {
        mojoNode.url.url = changes.url;
      }
    } else if (mojoNode.folder) {
      // Folder node type. We ignore the url field.
      if (changes.title !== undefined) {
        mojoNode.folder.title = changes.title;
      }
    } else {
      throw new Error('unexpected bookmark node type encountered');
    }

    const parentId = Store.getInstance().data.nodes[id]!.parentId!;
    return BookmarksService.getRemote().updateBookmarkNode(mojoNode).then(
        updated => {
          return normalizeMojoNode(updated, parentId);
        });
  }

  create(parentId: string, index: number|null, title: string, url?: string) {
    // The url vs folder is implicitly determined by whether or not url is
    // defined.
    const mojoNode = toMojoNode({id: '', title, url});
    return BookmarksService.getRemote()
        .createBookmarkNode({value: parentId}, index, mojoNode)
        .then(
            mojoResponseNode => normalizeMojoNode(mojoResponseNode, parentId));
  }

  delete(idList: string[]) {
    const uuids = idList.map(id => {
      return {value: id};
    });
    return BookmarksService.getRemote().deleteBookmarkNodes(uuids).then(
        () => {});
  }

  handleMojoEvents(events: BookmarksEvent[]) {
    for (const event of events) {
      const eventType = whichBookmarksEvent(event);
      switch (eventType) {
        case BookmarksEventFieldTags.ADDED:
          this.handleMojoAdded_(event.added!);
          break;
        case BookmarksEventFieldTags.REMOVED:
          this.handleMojoRemoved_(event.removed!);
          break;
        case BookmarksEventFieldTags.MOVED:
          this.handleMojoMoved_(event.moved!);
          break;
        case BookmarksEventFieldTags.CHANGED:
          this.handleMojoChanged_(event.changed!);
          break;
        default:
          assertNotReachedCase(eventType);
      }
    }
  }

  private handleMojoAdded_(event: BookmarkNodeCreated) {
    this.onCreated.forward(
        event.parentId.value, event.index,
        normalizeMojoNode(event.node, event.parentId.value));
  }

  private handleMojoRemoved_(event: BookmarkNodeRemoved) {
    const id = event.id.value;
    const store = Store.getInstance();
    const node = store.data.nodes[id];
    if (node) {
      const parentId = node.parentId!;
      const parentNode = store.data.nodes[parentId];
      const index = parentNode ? parentNode.children!.indexOf(id) : -1;
      this.onRemoved.forward(id, parentId, index);
    }
  }

  private handleMojoMoved_(event: BookmarkNodeMoved) {
    const oldParentId = event.oldParentId.value;
    const newParentId = event.newParentId.value;
    const store = Store.getInstance();
    const oldParentNode = store.data.nodes[oldParentId];
    // Folder node type is implicitly determined by existence of "children"
    // field.
    assert(
        !!oldParentNode && !!oldParentNode.children,
        'expected node: ' + oldParentId + ' to be a folder, but it is not');
    const id = oldParentNode.children[event.oldIndex];
    assert(
        id !== undefined,
        'expected to find [' + event.oldIndex + '], at folder: ' + oldParentId +
            ', but could not find a node at that index');
    this.onMoved.forward(
        id, oldParentId, event.oldIndex, newParentId, event.newIndex);
  }

  private handleMojoChanged_(event: BookmarkNodeChanged) {
    const id = event.node.url?.id?.value || event.node.folder?.id?.value || '';
    const targetNode = Store.getInstance().data.nodes[id];
    assert(!!targetNode, 'change target not found');
    const parentId = targetNode.parentId;
    // All non-root nodes should have a parentId and root nodes cannot be
    // changed
    assert(
        !!parentId,
        'target node does not have a parent, this is an invalid state');
    this.onChanged.forward(id, normalizeMojoNode(event.node, parentId));
  }

  static getInstance(): BookmarksApiProxy {
    return instance || (instance = new BookmarksApiProxyImpl());
  }

  static setInstance(obj: BookmarksApiProxy) {
    instance = obj;
  }
}

let instance: BookmarksApiProxy|null = null;
