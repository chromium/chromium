// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import '/strings.m.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {getCss as getSharedStyleCss} from 'chrome://resources/cr_elements/cr_shared_style_lit.css.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {highlightUpdatedItems, trackUpdatedItems} from './api_listener.js';
import {BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';
import {DialogFocusManager} from './dialog_focus_manager.js';
import {getHtml} from './edit_dialog.html.js';
import type {BookmarkNode} from './types.js';

export interface BookmarksEditDialogElement {
  $: {
    dialog: CrDialogElement,
    saveButton: HTMLElement,
    url: CrInputElement,
    name: CrInputElement,
  };
}

export class BookmarksEditDialogElement extends CrLitElement {
  static get is() {
    return 'bookmarks-edit-dialog';
  }

  static override get styles() {
    return getSharedStyleCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isFolder_: {type: Boolean},
      isEdit_: {type: Boolean},

      /**
       * Item that is being edited, or null when adding.
       */
      editItem_: {type: Object},

      /**
       * Parent node for the item being added, or null when editing.
       */
      parentId_: {type: String},
      titleValue_: {type: String},
      urlValue_: {type: String},
    };
  }

  protected accessor isFolder_: boolean = false;
  private accessor isEdit_: boolean = false;
  private accessor editItem_: BookmarkNode|null = null;
  private accessor parentId_: string|null = null;
  protected accessor titleValue_: string = '';
  protected accessor urlValue_: string = '';

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

  protected getDialogTitle_(): string {
    let title;
    if (this.isEdit_) {
      title = this.isFolder_ ? 'renameFolderTitle' : 'editBookmarkTitle';
    } else {
      title = this.isFolder_ ? 'addFolderTitle' : 'addBookmarkTitle';
    }

    return loadTimeData.getString(title);
  }

  protected onTitleValueChanged_(e: CustomEvent<{value: string}>) {
    this.titleValue_ = e.detail.value;
  }

  protected onUrlValueChanged_(e: CustomEvent<{value: string}>) {
    this.urlValue_ = e.detail.value;
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
    // Force an update to propagate this to the cr-input synchronously. This is
    // not best for performance, but validate() already forces an update to
    // the cr-input by calling performUpdate() on that element below, and this
    // method is not expected to be frequently called.
    this.performUpdate();

    if (urlInput.validate()) {
      return true;
    }

    this.urlValue_ = originalValue;
    return false;
  }

  protected onSaveButtonClick_() {
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

  protected onCancelButtonClick_() {
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
