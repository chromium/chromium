// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {BookmarksTreeNode} from './bookmarks.mojom-webui.js';
import type {PowerBookmarksEditDialogElement} from './power_bookmarks_edit_dialog.js';

export function getHtml(this: PowerBookmarksEditDialogElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog">
  <div slot="title">${this.getDialogTitle_()}</div>
  <div slot="body" class="body">
    <div class="input-section" ?hidden="${this.moveOnly_}">
      <div class="input-row name-input">
        <span class="input-label" aria-hidden="true">$i18n{editName}</span>
        <cr-input class="stroked" id="nameInput"
            .value="${this.getBookmarkName_()}"
            aria-label="$i18n{editName}"></cr-input>
      </div>
      <div class="input-row">
        <span class="input-label" aria-hidden="true">$i18n{editUrl}</span>
        <cr-input class="stroked" id="urlInput" type="url"
            .value="${this.getBookmarkUrl_()}"
            error-message="$i18n{editInvalidUrl}"
            aria-label="$i18n{editUrl}" required></cr-input>
      </div>
    </div>
    <sp-heading ?hide-back-button="${!this.activeFolderPath_.length}"
        back-button-title="$i18n{tooltipBack}"
        back-button-aria-label="${this.getBackButtonLabel_()}"
        @back-button-click="${this.onBackButtonClick_}">
      <h2 slot="heading">${this.getActiveFolderTitle_()}</h2>
    </sp-heading>
    <div id="folderSelector" class="folder-selector">
      ${this.showNewFolderInput_ ? html`
        <div class="folder-row">
          <div class="cr-icon icon-folder-open"></div>
          <cr-input class="stroked new-folder-input" id="newFolderInput"
              .value="${this.newFolderName_}"
              @value-changed="${this.onNewFolderNameValueChanged_}"
              @blur="${this.onBlur_}"
              @keydown="${this.onNewFolderInputKeydown_}"
              required></cr-input>
        </div>
      ` : ''}
      <cr-lazy-list id="folderList"
          .scrollTarget="${this.listScrollTarget_}"
          .items="${this.shownFolders_}"
          item-size="36"
          .template="${(item: BookmarksTreeNode, index: number) => html`
            <div class="folder-row"
                title="${item.title}"
                ?selected="${this.isSelected_(item)}"
                @click="${this.onFolderClick_}"
                data-index="${index}"
                tabindex="0">
              <div class="cr-icon icon-folder-open"></div>
              <div class="folder-title">
                ${item.title}
              </div>
              ${this.hasAvailableChildFolders_(item) ? html`
                <cr-icon-button class="subpage-arrow"
                    data-index="${index}"
                    title="${this.getForwardButtonTooltip_(item)}"
                    aria-label="${this.getForwardButtonLabel_(item)}"
                    @click="${this.onForwardClick_}">
                </cr-icon-button>
              ` : ''}
            </div>
          `}">
      </cr-lazy-list>
    </div>
  </div>
  <div class="button-row" slot="button-container">
    <cr-button id="newFolderButton" @click="${this.onNewFolderClick_}">
      $i18n{editNewFolder}
    </cr-button>
    <div>
      <cr-button @click="${this.onCancelClick_}">$i18n{editCancel}</cr-button>
      <cr-button id="saveFolderButton" class="action-button cr-button-gap"
          @click="${this.onSaveClick_}">
        $i18n{editSave}
      </cr-button>
    </div>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
  // clang-format on
}
