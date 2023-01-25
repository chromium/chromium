// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './power_bookmarks_edit_dialog.html.js';

export interface PowerBookmarksEditDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

function isFolder(node: chrome.bookmarks.BookmarkTreeNode) {
  return !node.url;
}

export class PowerBookmarksEditDialogElement extends PolymerElement {
  static get is() {
    return 'power-bookmarks-edit-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      topLevelBookmarks_: {
        type: Array,
        value: () => [],
      },

      selectedBookmarks_: {
        type: Array,
        value: () => [],
      },

      selectedFolder_: {
        type: Object,
        value: null,
      },

      activeFolder_: {
        type: Object,
        value: null,
      },
    };
  }

  private topLevelBookmarks_: chrome.bookmarks.BookmarkTreeNode[];
  private selectedBookmarks_: chrome.bookmarks.BookmarkTreeNode[];
  private selectedFolder_: chrome.bookmarks.BookmarkTreeNode|undefined;
  private activeFolder_: chrome.bookmarks.BookmarkTreeNode|undefined;

  showDialog(
      activeFolder: chrome.bookmarks.BookmarkTreeNode|undefined,
      topLevelBookmarks: chrome.bookmarks.BookmarkTreeNode[],
      selectedBookmarks: chrome.bookmarks.BookmarkTreeNode[]) {
    this.activeFolder_ = activeFolder;
    this.topLevelBookmarks_ = topLevelBookmarks;
    this.selectedBookmarks_ = selectedBookmarks;
    this.$.dialog.showModal();
  }

  private getActiveFolderTitle_() {
    if (this.activeFolder_ &&
        this.activeFolder_.id !== loadTimeData.getString('otherBookmarksId') &&
        this.activeFolder_.id !== loadTimeData.getString('mobileBookmarksId')) {
      return this.activeFolder_!.title;
    } else {
      return loadTimeData.getString('allBookmarks');
    }
  }

  private getShownFolders_(): chrome.bookmarks.BookmarkTreeNode[] {
    if (this.activeFolder_ && this.activeFolder_.children) {
      return this.activeFolder_.children!.filter(isFolder);
    } else if (!this.activeFolder_ && this.topLevelBookmarks_) {
      return this.topLevelBookmarks_.filter(isFolder);
    }
    assertNotReached('No bookmarks to display in edit menu');
  }

  private onNewFolder_() {
    // TODO
  }

  private onCancel_() {
    this.$.dialog.close();
  }

  private onSave_() {
    // TODO
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'power-bookmarks-edit-dialog': PowerBookmarksEditDialogElement;
  }
}

customElements.define(
    PowerBookmarksEditDialogElement.is, PowerBookmarksEditDialogElement);
