// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './strings.m.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {highlightUpdatedItems, trackUpdatedItems} from './api_listener.js';
import {BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';
import {DialogFocusManager} from './dialog_focus_manager.js';
import {getTemplate} from './edit_dialog.html.js';
import type {BookmarkNode} from './types.js';

export interface BookmarksEditDialogElement {
  $: {
    dialog: CrDialogElement,
    saveButton: HTMLElement,
    url: CrInputElement,
    name: CrInputElement,
  };
}

export class BookmarksEditDialogElement extends PolymerElement {
  static get is() {
    return 'bookmarks-edit-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isFolder_: Boolean,
      isEdit_: Boolean,

      /**
       * Item that is being edited, or null when adding.
       */
      editItem_: Object,

      /**
       * Parent node for the item being added, or null when editing.
       */
      parentId_: String,
      titleValue_: String,
      urlValue_: String,
    };
  }

  private isFolder_: boolean;
  private isEdit_: boolean;
  private editItem_: BookmarkNode|null;
  private parentId_: string|null;
  private titleValue_: string;
  private urlValue_: string;

  /**
   * Show the dialog to add a new folder (if |isFolder|) or item, which will be
   * inserted into the tree as a child of |parentId|.
   */
  showAddDialog(isFolder: boolean, parentId: string) {
    this.reset_();
    this.isEdit_ = false;
    this.isFolder_ = isFolder;
    this.parentId_ = parentId;

    DialogFocusManager.getInstance().showDialog(this.$.dialog);
  }

  /** Show the edit dialog for |editItem|. */
  showEditDialog(editItem: BookmarkNode) {
    this.reset_();
    this.isEdit_ = true;
    this.isFolder_ = !editItem.url;
    this.editItem_ = editItem;

    this.titleValue_ = editItem.title;
    if (!this.isFolder_) {
      assert(editItem.url);
      this.urlValue_ = editItem.url;
    }

    DialogFocusManager.getInstance().showDialog(this.$.dialog);
  }

  /**
   * Clear out existing values from the dialog, allowing it to be reused.
   */
  private reset_() {
    this.editItem_ = null;
    this.parentId_ = null;
    this.$.url.invalid = false;
    this.titleValue_ = '';
    this.urlValue_ = '';
  }

  private getDialogTitle_(isFolder: boolean, isEdit: boolean): string {
    let title;
    if (isEdit) {
      title = isFolder ? 'renameFolderTitle' : 'editBookmarkTitle';
    } else {
      title = isFolder ? 'addFolderTitle' : 'addBookmarkTitle';
    }

    return loadTimeData.getString(title);
  }

  /**
   * Validates the value of the URL field, returning true if it is a valid URL.
   * May modify the value by prepending 'http://' in order to make it valid.
   * Note: Made public only for the purposes of testing.
   */
  validateUrl(): boolean {
    const urlInput = this.$.url;

    if (urlInput.validate()) {
      return true;
    }

    const originalValue = this.urlValue_;
    this.urlValue_ = 'http://' + originalValue;

    if (urlInput.validate()) {
      return true;
    }

    this.urlValue_ = originalValue;
    return false;
  }

  private onSaveButtonClick_() {
    const edit: { title: string, url?: string, parentId?: string|null } =
        { 'title': this.titleValue_ };
    if (!this.isFolder_) {
      if (!this.validateUrl()) {
        return;
      }

      edit['url'] = this.urlValue_;
    }

    if (this.isEdit_) {
      chrome.bookmarks.update(this.editItem_!.id, edit);
    } else {
      edit['parentId'] = this.parentId_;
      trackUpdatedItems();
      BookmarksApiProxyImpl.getInstance().create(edit).then(
          highlightUpdatedItems);
    }
    this.$.dialog.close();
  }

  private onCancelButtonClick_() {
    this.$.dialog.cancel();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'bookmarks-edit-dialog': BookmarksEditDialogElement;
  }
}

customElements.define(
    BookmarksEditDialogElement.is, BookmarksEditDialogElement);
