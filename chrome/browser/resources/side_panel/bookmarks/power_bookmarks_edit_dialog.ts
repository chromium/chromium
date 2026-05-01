// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_lazy_list/cr_lazy_list.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {BookmarksTreeNode} from './bookmarks.mojom-webui.js';
import {getCss} from './power_bookmarks_edit_dialog.css.js';
import {getHtml} from './power_bookmarks_edit_dialog.html.js';
import {getFolderDescendants} from './power_bookmarks_service.js';

export const TEMP_FOLDER_ID_PREFIX = 'tmp_new_folder_';

export interface PowerBookmarksEditDialogElement {
  $: {
    dialog: CrDialogElement,
    nameInput: CrInputElement,
    urlInput: CrInputElement,
    folderSelector: HTMLElement,
  };
}

export class PowerBookmarksEditDialogElement extends CrLitElement {
  static get is() {
    return 'power-bookmarks-edit-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      topLevelBookmarks_: {type: Array},
      selectedBookmarks_: {type: Array},
      selectedFolder_: {type: Object},
      activeFolderPath_: {type: Array},
      newFolders_: {type: Array},
      moveOnly_: {type: Boolean},
      newFolderName_: {type: String},
      showNewFolderInput_: {type: Boolean},
      shownFolders_: {type: Array},
      listScrollTarget_: {type: Object},
    };
  }

  protected accessor topLevelBookmarks_: BookmarksTreeNode[] = [];
  protected accessor selectedBookmarks_: BookmarksTreeNode[] = [];
  protected accessor selectedFolder_: BookmarksTreeNode|undefined = undefined;
  protected accessor activeFolderPath_: BookmarksTreeNode[] = [];
  protected accessor newFolders_: BookmarksTreeNode[] = [];
  protected accessor moveOnly_: boolean = false;
  protected accessor newFolderName_: string = '';
  protected accessor showNewFolderInput_: boolean = false;
  protected accessor shownFolders_: BookmarksTreeNode[] = [];
  protected accessor listScrollTarget_: HTMLElement = this;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('activeFolderPath_') ||
        changedPrivateProperties.has('topLevelBookmarks_') ||
        changedPrivateProperties.has('selectedBookmarks_') ||
        changedPrivateProperties.has('newFolders_')) {
      this.shownFolders_ = this.computeShownFolders_();
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.listScrollTarget_ = this.$.folderSelector;
  }

  showDialog(
      activeFolderPath: BookmarksTreeNode[],
      topLevelBookmarks: BookmarksTreeNode[],
      selectedBookmarks: BookmarksTreeNode[], moveOnly: boolean) {
    this.activeFolderPath_ = activeFolderPath.slice();
    this.topLevelBookmarks_ = topLevelBookmarks;
    this.selectedBookmarks_ = selectedBookmarks;
    this.newFolders_ = [];
    this.moveOnly_ = moveOnly;
    this.$.dialog.showModal();
    this.newFolderName_ = loadTimeData.getString('newFolderTitle');
    this.showNewFolderInput_ = false;
  }

  private isAvailableFolder_(node: BookmarksTreeNode): boolean {
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

  protected getDialogTitle_(): string {
    if (this.moveOnly_) {
      return loadTimeData.getString('editMoveFolderTo');
    } else {
      return loadTimeData.getString('editBookmark');
    }
  }

  protected getBookmarkName_(): string {
    if (this.selectedBookmarks_.length === 1) {
      return this.selectedBookmarks_[0].title;
    }
    return '';
  }

  protected getBookmarkUrl_(): string {
    if (this.selectedBookmarks_.length === 1) {
      return this.selectedBookmarks_[0].url!;
    }
    return '';
  }

  private getActiveFolder_(): BookmarksTreeNode|undefined {
    if (this.activeFolderPath_.length) {
      return this.activeFolderPath_[this.activeFolderPath_.length - 1];
    }
    return undefined;
  }

  protected getActiveFolderTitle_(): string {
    return this.getFolderTitle_(this.getActiveFolder_());
  }

  private getFolderTitle_(folder: BookmarksTreeNode|undefined): string {
    if (folder && folder.id !== loadTimeData.getString('otherBookmarksId') &&
        folder.id !== loadTimeData.getString('mobileBookmarksId')) {
      return folder.title;
    } else {
      return loadTimeData.getString('allBookmarks');
    }
  }

  private computeShownFolders_(): BookmarksTreeNode[] {
    const activeFolder = this.getActiveFolder_();
    if (activeFolder && activeFolder.children) {
      return activeFolder.children.filter(this.isAvailableFolder_, this);
    } else if (!activeFolder && this.topLevelBookmarks_) {
      return this.topLevelBookmarks_.filter(this.isAvailableFolder_, this);
    }
    assertNotReached('No bookmarks to display in edit menu');
  }

  protected getBackButtonLabel_(): string {
    let activeFolderParent: BookmarksTreeNode|undefined;
    if (this.activeFolderPath_.length > 1) {
      activeFolderParent =
          this.activeFolderPath_[this.activeFolderPath_.length - 2];
    }
    return loadTimeData.getStringF(
        'backButtonLabel', this.getFolderTitle_(activeFolderParent));
  }

  protected getForwardButtonTooltip_(folder: BookmarksTreeNode): string {
    return loadTimeData.getStringF(
        'openBookmarkLabel', this.getFolderTitle_(folder));
  }

  protected getForwardButtonLabel_(folder: BookmarksTreeNode): string {
    return loadTimeData.getStringF(
        'forwardButtonLabel', this.getFolderTitle_(folder));
  }

  protected hasAvailableChildFolders_(folder: BookmarksTreeNode): boolean {
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

  protected isSelected_(folder: BookmarksTreeNode): boolean {
    return folder === this.selectedFolder_;
  }

  private getShownFolder_(event: Event): BookmarksTreeNode {
    const target = event.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    return this.shownFolders_[index];
  }

  protected onBackButtonClick_() {
    this.selectedFolder_ = undefined;
    this.activeFolderPath_.pop();
    this.requestUpdate();
  }

  protected onForwardClick_(event: MouseEvent) {
    this.selectedFolder_ = undefined;
    const folder = this.getShownFolder_(event);
    this.activeFolderPath_.push(folder);
    this.requestUpdate();
  }

  protected onFolderClick_(event: MouseEvent) {
    const folder = this.getShownFolder_(event);
    if (this.selectedFolder_ === folder) {
      this.selectedFolder_ = undefined;
    } else {
      this.selectedFolder_ = folder;
    }
  }

  protected onNewFolderClick_() {
    this.showNewFolderInput_ = true;
  }

  protected onNewFolderInputDomChange_() {
    const input =
        this.shadowRoot.querySelector<CrInputElement>('#newFolderInput');
    if (!input) {
      return;
    }
    input.select();
  }

  protected onNewFolderNameValueChanged_(event: CustomEvent<{value: string}>):
      void {
    this.newFolderName_ = event.detail.value;
  }

  protected onNewFolderInputKeydown_(event: KeyboardEvent): void {
    /**
     * This key down listener overrides the existing behaviour where the
     * parent dialog would close on 'Enter'.
     */
    if (event.key === 'Enter') {
      event.preventDefault();
      event.stopPropagation();
      this.saveNewFolder_();

      const saveButton =
          this.shadowRoot.querySelector<CrButtonElement>('#saveFolderButton');

      assert(!!saveButton);
      saveButton.focus();
    }
  }

  protected onBlur_(event: KeyboardEvent): void {
    /**
     * This prevents the blur event from being called when the save button is
     * focused when the enter key is pressed.
     */
    if (!this.showNewFolderInput_) {
      return;
    }
    event.preventDefault();
    event.stopPropagation();
    this.saveNewFolder_();
  }

  private saveNewFolder_() {
    this.showNewFolderInput_ = false;

    const parent =
        this.selectedFolder_ ? this.selectedFolder_ : this.getActiveFolder_();
    const parentId =
        parent ? parent.id : loadTimeData.getString('otherBookmarksId');
    const newFolder: BookmarksTreeNode = {
      id: TEMP_FOLDER_ID_PREFIX + this.newFolders_.length,
      title: this.newFolderName_,
      index: 0,
      url: null,
      children: [],
      parentId: parentId,
      dateAdded: null,
      dateLastUsed: null,
      unmodifiable: false,
    };
    if (parent) {
      parent.children!.unshift(newFolder);
    } else {
      this.topLevelBookmarks_.unshift(newFolder);
    }
    this.newFolders_.push(newFolder);
    if (parent !== this.getActiveFolder_()) {
      this.activeFolderPath_.push(parent!);
    }
    this.requestUpdate();
    this.selectedFolder_ = newFolder;

    this.newFolderName_ = loadTimeData.getString('newFolderTitle');
  }

  protected onCancelClick_() {
    this.close_();
  }

  protected onSaveClick_() {
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
    this.fire('save', {
      bookmarks: this.selectedBookmarks_,
      name: this.moveOnly_ ? undefined : this.$.nameInput.inputElement.value,
      url: this.moveOnly_ ? undefined : this.$.urlInput.inputElement.value,
      folderId: folderId,
      newFolders: this.newFolders_,
    });
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
