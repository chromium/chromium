// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './bookmark_tree_node.js';

import {assertNotReachedCase} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BookmarksEventFieldTags, BookmarksObserverReceiver, BookmarksService, whichBookmarksEvent} from '../bookmarks_api.mojom-webui.js';
import type {BookmarkNode, BookmarkNodeChanged, BookmarkNodeCreated, BookmarkNodeMoved, BookmarkNodeRemoved, BookmarksEvent, BookmarksObserverInterface} from '../bookmarks_api.mojom-webui.js';

import type {BookmarkTreeNodeElement} from './bookmark_tree_node.js';
import {getCss} from './bookmarks.css.js';
import {getHtml} from './bookmarks.html.js';

export class BookmarksElement extends CrLitElement implements
    BookmarksObserverInterface {
  static get is() {
    return 'webui-browser-bookmarks';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      rootNode_: {type: Object},
    };
  }

  protected accessor rootNode_: BookmarkNode = {
    folder: {
      id: {value: ''},
      title: '',
      children: [],
      permanentFolderType: null,
      isSynced: false,
      legacy: null,
    },
  };

  private receiver_: BookmarksObserverReceiver =
      new BookmarksObserverReceiver(this);

  override connectedCallback() {
    super.connectedCallback();
    this.fetchBookmarks_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.receiver_.$.close();
  }

  private fetchBookmarks_() {
    BookmarksService.getRemote().getBookmarks().then(snapshot => {
      this.rootNode_ = {
        folder: {
          id: snapshot.root.id,
          title: '',
          children: snapshot.root.children.map(folder => ({folder})),
          permanentFolderType: null,
          isSynced: false,
          legacy: null,
        },
      };
      this.receiver_.$.bindHandle(snapshot.stream.handle);
    });
  }

  // BookmarksObserverInterface implementation:
  onBookmarksEvents(events: BookmarksEvent[]) {
    for (const event of events) {
      const which = whichBookmarksEvent(event);
      switch (which) {
        case BookmarksEventFieldTags.ADDED:
          this.onBookmarkNodeAdded_(event.added!);
          break;
        case BookmarksEventFieldTags.REMOVED:
          this.onBookmarkNodeRemoved_(event.removed!);
          break;
        case BookmarksEventFieldTags.MOVED:
          this.onBookmarkNodeMoved_(event.moved!);
          break;
        case BookmarksEventFieldTags.CHANGED:
          this.onBookmarkNodeChanged_(event.changed!);
          break;
        default:
          assertNotReachedCase(which);
      }
    }
  }

  private onBookmarkNodeAdded_(event: BookmarkNodeCreated) {
    const parentElement = this.findNodeElement_(event.parentId.value);
    if (parentElement && parentElement.node.folder) {
      const folder = parentElement.node.folder;
      const children = [...(folder.children || [])];
      children.splice(event.index, 0, event.node);
      parentElement.node = {
        folder: {
          ...folder,
          children: children,
        },
      };
    }
  }

  private onBookmarkNodeRemoved_(event: BookmarkNodeRemoved) {
    const childElement = this.findNodeElement_(event.id.value);
    if (childElement) {
      const shadowRoot = childElement.getRootNode() as ShadowRoot;
      const parentElement = shadowRoot.host as BookmarkTreeNodeElement;
      if (parentElement && parentElement.node.folder) {
        const folder = parentElement.node.folder;
        const children = folder.children.filter(
            child => (child.url?.id?.value || child.folder?.id?.value) !==
                event.id.value);
        parentElement.node = {
          folder: {
            ...folder,
            children: children,
          },
        };
      }
    }
  }

  private onBookmarkNodeMoved_(event: BookmarkNodeMoved) {
    const oldParentElement = this.findNodeElement_(event.oldParentId.value);
    const newParentElement = this.findNodeElement_(event.newParentId.value);

    if (oldParentElement && oldParentElement.node.folder && newParentElement &&
        newParentElement.node.folder) {
      const oldFolder = oldParentElement.node.folder;
      const oldChildren = [...oldFolder.children];
      const [movedNode] = oldChildren.splice(event.oldIndex, 1);

      if (movedNode) {
        if (oldParentElement === newParentElement) {
          oldChildren.splice(event.newIndex, 0, movedNode);
          oldParentElement.node = {
            folder: {
              ...oldFolder,
              children: oldChildren,
            },
          };
        } else {
          oldParentElement.node = {
            folder: {
              ...oldFolder,
              children: oldChildren,
            },
          };

          const newFolder = newParentElement.node.folder;
          const newChildren = [...(newFolder.children || [])];
          newChildren.splice(event.newIndex, 0, movedNode);
          newParentElement.node = {
            folder: {
              ...newFolder,
              children: newChildren,
            },
          };
        }
      }
    }
  }

  private onBookmarkNodeChanged_(event: BookmarkNodeChanged) {
    const nodeId =
        event.node.url?.id?.value || event.node.folder?.id?.value || '';
    const element = this.findNodeElement_(nodeId);
    if (element) {
      element.node = event.node;
    }
  }

  private findNodeElement_(id: string): BookmarkTreeNodeElement|null {
    return findNodeElement(this.shadowRoot, id);
  }
}

function findNodeElement(root: ShadowRoot|HTMLElement|null, id: string):
    BookmarkTreeNodeElement|null {
  if (!root) {
    return null;
  }
  const el = root.querySelector(`[id="${id}"]`);
  if (el) {
    return el as BookmarkTreeNodeElement;
  }
  const treeNodes = root.querySelectorAll('webui-browser-bookmark-tree-node');
  for (const node of treeNodes) {
    if (node.shadowRoot) {
      const found = findNodeElement(node.shadowRoot, id);
      if (found) {
        return found;
      }
    }
  }
  return null;
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-bookmarks': BookmarksElement;
  }
}

customElements.define(BookmarksElement.is, BookmarksElement);
