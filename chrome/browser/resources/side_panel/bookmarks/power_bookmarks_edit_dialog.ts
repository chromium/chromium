// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './power_bookmarks_edit_dialog.html.js';
import {getFolderDescendants} from './power_bookmarks_service.js';

export const TEMP_FOLDER_ID_PREFIX = 'tmp_new_folder_';

export interface PowerBookmarksEditDialogElement {
  $: {
    dialog: CrDialogElement,
    nameInput: CrInputElement,
    urlInput: CrInputElement,
  };
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

      moveOnly_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private topLevelBookmarks_: chrome.bookmarks.BookmarkTreeNode[];
  private selectedBookmarks_: chrome.bookmarks.BookmarkTreeNode[];
  private selectedFolder_: chrome.bookmarks.BookmarkTreeNode|undefined;
  private activeFolderPath_: chrome.bookmarks.BookmarkTreeNode[];
  private newFolders_: chrome.bookmarks.BookmarkTreeNode[];
  private moveOnly_: boolean;

  showDialog(
      activeFolderPath: chrome.bookmarks.BookmarkTreeNode[],
      topLevelBookmarks: chrome.bookmarks.BookmarkTreeNode[],
      selectedBookmarks: chrome.bookmarks.BookmarkTreeNode[],
      moveOnly: boolean) {
    this.activeFolderPath_ = activeFolderPath.slice();
    this.topLevelBookmarks_ = topLevelBookmarks;
    this.selectedBookmarks_ = selectedBookmarks;
    this.newFolders_ = [];
    this.moveOnly_ = moveOnly;
    this.$.dialog.showModal();
  }

  private isAvailableFolder_(node: chrome.bookmarks.BookmarkTreeNode): boolean {
    if (node.url) {
      return false;
    }
    for (const selectedBookmark of this.selectedBookmarks_) {
      // Don't allow moving a folder to itself or any of its descendants.
      const descendants = getFolderDescendants(selectedBookmark);
      if (descendants.includes(node)) {
        return false;
      }
    }
    return true;
  }

  private getDialogTitle_(): string {
    if (this.moveOnly_) {
      return loadTimeData.getString('editMoveFolderTo');
    } else {
      return loadTimeData.getString('editBookmark');
    }
  }

  private getBookmarkName_(): string {
    if (this.selectedBookmarks_.length === 1) {
      return this.selectedBookmarks_[0]!.title;
    }
    return '';
  }

  private getBookmarkUrl_(): string {
    if (this.selectedBookmarks_.length === 1) {
      return this.selectedBookmarks_[0]!.url!;
    }
    return '';
  }

  private getActiveFolder_(): chrome.bookmarks.BookmarkTreeNode|undefined {
    if (this.activeFolderPath_.length) {
      return this.activeFolderPath_[this.activeFolderPath_.length - 1];
    }
    return undefined;
  }

  private getActiveFolderTitle_(): string {
    return this.getFolderTitle_(this.getActiveFolder_());
  }

  private getFolderTitle_(folder: chrome.bookmarks.BookmarkTreeNode|
                          undefined): string {
    if (folder && folder.id !== loadTimeData.getString('otherBookmarksId') &&
        folder.id !== loadTimeData.getString('mobileBookmarksId')) {
      return folder!.title;
    } else {
      return loadTimeData.getString('allBookmarks');
    }
  }

  private getShownFolders_(): chrome.bookmarks.BookmarkTreeNode[] {
    const activeFolder = this.getActiveFolder_();
    if (activeFolder && activeFolder.children) {
      return activeFolder.children!.filter(this.isAvailableFolder_, this);
    } else if (!activeFolder && this.topLevelBookmarks_) {
      return this.topLevelBookmarks_.filter(this.isAvailableFolder_, this);
    }
    assertNotReached('No bookmarks to display in edit menu');
  }

  private getBackButtonLabel_(): string {
    let activeFolderParent: chrome.bookmarks.BookmarkTreeNode|undefined;
    if (this.activeFolderPath_.length > 1) {
      activeFolderParent =
          this.activeFolderPath_[this.activeFolderPath_.length - 2];
    }
    return loadTimeData.getStringF(
        'backButtonLabel', this.getFolderTitle_(activeFolderParent));
  }

  private getForwardButtonTooltip_(folder: chrome.bookmarks.BookmarkTreeNode):
      string {
    return loadTimeData.getStringF(
        'openBookmarkLabel', this.getFolderTitle_(folder));
  }

  private getForwardButtonLabel_(folder: chrome.bookmarks.BookmarkTreeNode):
      string {
    return loadTimeData.getStringF(
        'forwardButtonLabel', this.getFolderTitle_(folder));
  }

  private hasAvailableChildFolders_(folder: chrome.bookmarks.BookmarkTreeNode):
      boolean {
    return folder.children!.filter(this.isAvailableFolder_, this).length > 0;
  }

  private validateUrl_(): boolean {
    const urlInput = this.$.urlInput;

    if (urlInput.validate()) {
      return true;
    }

    const originalValue = urlInput.inputElement.value;
    urlInput.inputElement.value = 'http://' + originalValue;

    if (urlInput.validate()) {
      return true;
    }

    urlInput.inputElement.value = originalValue;
    return false;
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
    if (this.selectedFolder_ === event.model.item) {
      this.selectedFolder_ = undefined;
    } else {
      this.selectedFolder_ = event.model.item;
    }
  }

  private onNewFolder_() {
    const parent =
        this.selectedFolder_ ? this.selectedFolder_ : this.getActiveFolder_();
    const parentId =
        parent ? parent.id : loadTimeData.getString('otherBookmarksId');
    const newFolder: chrome.bookmarks.BookmarkTreeNode = {
      id: TEMP_FOLDER_ID_PREFIX + this.newFolders_.length,
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
    if (!this.moveOnly_ && !this.validateUrl_()) {
      return;
    }
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
        bookmarks: this.selectedBookmarks_,
        name: this.moveOnly_ ? undefined : this.$.nameInput.inputElement.value,
        url: this.moveOnly_ ? undefined : this.$.urlInput.inputElement.value,
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
