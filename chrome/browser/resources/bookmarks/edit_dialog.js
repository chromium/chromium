// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {highlightUpdatedItems, trackUpdatedItems} from './api_listener.js';
import {DialogFocusManager} from './dialog_focus_manager.js';
import {BookmarkNode} from './types.js';

/** @polymer */
export class BookmarksEditDialogElement extends PolymerElement {
  static get is() {
    return 'bookmarks-edit-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      isFolder_: Boolean,

      /** @private */
      isEdit_: Boolean,

      /**
       * Item that is being edited, or null when adding.
       * @private {?BookmarkNode}
       */
      editItem_: Object,

      /**
       * Parent node for the item being added, or null when editing.
       * @private {?string}
       */
      parentId_: String,

      /** @private */
      titleValue_: String,

      /** @private */
      urlValue_: String,
    };
  }

  /**
   * Show the dialog to add a new folder (if |isFolder|) or item, which will be
   * inserted into the tree as a child of |parentId|.
   * @param {boolean} isFolder
   * @param {string} parentId
   */
  showAddDialog(isFolder, parentId) {
    this.reset_();
    this.isEdit_ = false;
    this.isFolder_ = isFolder;
    this.parentId_ = parentId;

    DialogFocusManager.getInstance().showDialog(
        /** @type {!HTMLDialogElement} */ (this.$.dialog));
  }

  /**
   * Show the edit dialog for |editItem|.
   * @param {BookmarkNode} editItem
   */
  showEditDialog(editItem) {
    this.reset_();
    this.isEdit_ = true;
    this.isFolder_ = !editItem.url;
    this.editItem_ = editItem;

    this.titleValue_ = editItem.title;
    if (!this.isFolder_) {
      this.urlValue_ = assert(editItem.url);
    }

    DialogFocusManager.getInstance().showDialog(
        /** @type {!HTMLDialogElement} */ (this.$.dialog));
  }

  /**
   * Clear out existing values from the dialog, allowing it to be reused.
   * @private
   */
  reset_() {
    this.editItem_ = null;
    this.parentId_ = null;
    this.$.url.invalid = false;
    this.titleValue_ = '';
    this.urlValue_ = '';
  }

  /**
   * @param {boolean} isFolder
   * @param {boolean} isEdit
   * @return {string}
   * @private
   */
  getDialogTitle_(isFolder, isEdit) {
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
   * @return {boolean}
   * @private
   */
  validateUrl_() {
    const urlInput = /** @type {CrInputElement} */ (this.$.url);
    const originalValue = this.urlValue_;

    if (urlInput.validate()) {
      return true;
    }

    this.urlValue_ = 'http://' + originalValue;

    if (urlInput.validate()) {
      return true;
    }

    this.urlValue_ = originalValue;
    return false;
  }

  /** @private */
  onSaveButtonTap_() {
    const edit = {'title': this.titleValue_};
    if (!this.isFolder_) {
      if (!this.validateUrl_()) {
        return;
      }

      edit['url'] = this.urlValue_;
    }

    if (this.isEdit_) {
      chrome.bookmarks.update(this.editItem_.id, edit);
    } else {
      edit['parentId'] = this.parentId_;
      trackUpdatedItems();
      chrome.bookmarks.create(edit, highlightUpdatedItems);
    }
    this.$.dialog.close();
  }

  /** @private */
  onCancelButtonTap_() {
    this.$.dialog.cancel();
  }
}

customElements.define(
    BookmarksEditDialogElement.is, BookmarksEditDialogElement);
