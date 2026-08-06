// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BookmarksService} from '../bookmarks_api.mojom-webui.js';
import type {BookmarkNode} from '../bookmarks_api.mojom-webui.js';

import {getCss} from './bookmark_tree_node.css.js';
import {getHtml} from './bookmark_tree_node.html.js';

// TODO(crbug.com/501483829): split this up into two different types.
export class BookmarkTreeNodeElement extends CrLitElement {
  static get is() {
    return 'webui-browser-bookmark-tree-node';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      node: {type: Object},
    };
  }

  accessor node: BookmarkNode = {
    folder: {
      id: {value: ''},
      title: '',
      children: [],
      permanentFolderType: null,
      isSynced: false,
      legacy: null,
    },
  };

  private bookmarksService_ = BookmarksService.getRemote();

  protected onAddUrlClick(e: Event) {
    e.stopPropagation();
    e.preventDefault();
    if (!this.node.folder) {
      return;
    }

    const parentId = this.node.folder.id!;
    const newUrlNode = {
      url: {
        id: null,
        title: 'new bookmark',
        url: 'chrome://new-tab-page',
        faviconUrl: null,
        isSynced: false,
        legacy: null,
      },
    };

    this.bookmarksService_.createBookmarkNode(parentId, null, newUrlNode);
  }

  protected onAddFolderClick(e: Event) {
    e.stopPropagation();
    e.preventDefault();
    if (!this.node.folder) {
      return;
    }

    const parentId = this.node.folder.id!;
    const newFolderNode = {
      folder: {
        id: null,
        title: 'new folder',
        children: [],
        permanentFolderType: null,
        isSynced: false,
        legacy: null,
      },
    };

    this.bookmarksService_.createBookmarkNode(parentId, null, newFolderNode);
  }

  protected onEditClick(e: Event) {
    e.stopPropagation();
    e.preventDefault();

    if (this.node.url) {
      const updatedNode = {
        url: {
          id: this.node.url.id!,
          title: 'has been updated',
          url: 'http://updated.somewhere',
          faviconUrl: null,
          isSynced: false,
          legacy: null,
        },
      };

      this.bookmarksService_.updateBookmarkNode(updatedNode);
    } else if (this.node.folder) {
      const updatedNode = {
        folder: {
          id: this.node.folder.id!,
          title: 'updated folder',
          children: [],
          permanentFolderType: null,
          isSynced: false,
          legacy: null,
        },
      };

      this.bookmarksService_.updateBookmarkNode(updatedNode);
    }
  }

  protected onMoveClick(e: Event) {
    e.stopPropagation();
    e.preventDefault();

    const host = (this.getRootNode() as ShadowRoot).host;

    if (host &&
        host.tagName.toLowerCase() === 'webui-browser-bookmark-tree-node') {
      const id = this.node.url ? this.node.url.id! : this.node.folder!.id!;

      const targetParentId = (host as BookmarkTreeNodeElement).node.folder!.id!;
      // always move to first element for now.
      this.bookmarksService_.moveBookmarkNode(id, targetParentId, 0);
    }
  }

  protected onDeleteClick(e: Event) {
    e.stopPropagation();
    e.preventDefault();

    const id = this.node.url ? this.node.url.id! : this.node.folder!.id!;

    this.bookmarksService_.deleteBookmarkNodes([id]);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-bookmark-tree-node': BookmarkTreeNodeElement;
  }
}

customElements.define(BookmarkTreeNodeElement.is, BookmarkTreeNodeElement);
