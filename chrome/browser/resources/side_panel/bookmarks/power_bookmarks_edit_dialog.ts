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
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

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

      activeFolderPath_: {
        type: Array,
        value: () => [],
      },

      newFolders_: {
        type: Array,
        value: () => [],
      },
    };
  }

  private topLevelBookmarks_: chrome.bookmarks.BookmarkTreeNode[];
  private selectedBookmarks_: chrome.bookmarks.BookmarkTreeNode[];
  private selectedFolder_: chrome.bookmarks.BookmarkTreeNode|undefined;
  private activeFolderPath_: chrome.bookmarks.BookmarkTreeNode[];
  private newFolders_: chrome.bookmarks.BookmarkTreeNode[];

  showDialog(
      activeFolderPath: chrome.bookmarks.BookmarkTreeNode[],
      topLevelBookmarks: chrome.bookmarks.BookmarkTreeNode[],
      selectedBookmarks: chrome.bookmarks.BookmarkTreeNode[]) {
    this.activeFolderPath_ = activeFolderPath.slice();
    this.topLevelBookmarks_ = topLevelBookmarks;
    this.selectedBookmarks_ = selectedBookmarks;
    this.newFolders_ = [];
    this.$.dialog.showModal();
  }

  private getActiveFolder_(): chrome.bookmarks.BookmarkTreeNode|undefined {
    if (this.activeFolderPath_.length) {
      return this.activeFolderPath_[this.activeFolderPath_.length - 1];
    }
    return undefined;
  }

  private getActiveFolderTitle_() {
    const activeFolder = this.getActiveFolder_();
    if (activeFolder &&
        activeFolder.id !== loadTimeData.getString('otherBookmarksId') &&
        activeFolder.id !== loadTimeData.getString('mobileBookmarksId')) {
      return activeFolder!.title;
    } else {
      return loadTimeData.getString('allBookmarks');
    }
  }

  private getShownFolders_(): chrome.bookmarks.BookmarkTreeNode[] {
    const activeFolder = this.getActiveFolder_();
    if (activeFolder && activeFolder.children) {
      return activeFolder.children!.filter(isFolder);
    } else if (!activeFolder && this.topLevelBookmarks_) {
      return this.topLevelBookmarks_.filter(isFolder);
    }
    assertNotReached('No bookmarks to display in edit menu');
  }

  private hasChildFolders_(folder: chrome.bookmarks.BookmarkTreeNode): boolean {
    return folder.children!.filter(isFolder).length > 0;
  }

  private isSelected_(folder: chrome.bookmarks.BookmarkTreeNode): boolean {
    return folder === this.selectedFolder_;
  }

  private onBack_() {
    this.selectedFolder_ = undefined;
    this.pop('activeFolderPath_');
  }

  private onForward_(event: DomRepeatEvent<chrome.bookmarks.BookmarkTreeNode>) {
    this.selectedFolder_ = undefined;
    this.push('activeFolderPath_', event.model.item);
  }

  private onFolderSelected_(
      event: DomRepeatEvent<chrome.bookmarks.BookmarkTreeNode>) {
    this.selectedFolder_ = event.model.item;
  }

  private onNewFolder_() {
    const parent =
        this.selectedFolder_ ? this.selectedFolder_ : this.getActiveFolder_();
    const parentId =
        parent ? parent.id : loadTimeData.getString('otherBookmarksId');
    const newFolder: chrome.bookmarks.BookmarkTreeNode = {
      id: 'tmp_new_folder_' + this.newFolders_.length,
      title: loadTimeData.getString('newFolderTitle'),
      children: [],
      parentId: parentId,
    };
    if (parent) {
      parent.children!.unshift(newFolder);
    } else {
      this.topLevelBookmarks_.unshift(newFolder);
    }
    this.push('newFolders_', newFolder);
    if (parent !== this.getActiveFolder_()) {
      this.push('activeFolderPath_', parent);
    }
    this.selectedFolder_ = newFolder;
  }

  private onCancel_() {
    this.close_();
  }

  private onSave_() {
    const activeFolder = this.getActiveFolder_();
    let folderId;
    if (this.selectedFolder_) {
      folderId = this.selectedFolder_.id;
    } else if (activeFolder) {
      folderId = activeFolder.id;
    } else {
      folderId = loadTimeData.getString('otherBookmarksId');
    }
    this.dispatchEvent(new CustomEvent('save', {
      bubbles: true,
      composed: true,
      detail: {
        folderId: folderId,
        newFolders: this.newFolders_,
      },
    }));
    this.close_();
  }

  private close_() {
    this.selectedFolder_ = undefined;
    this.newFolders_ = [];
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
