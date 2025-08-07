// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerSaveToDriveBubbleElement} from './viewer_save_to_drive_bubble.js';

// TODO(crbug.com/427451594): Hook up the buttons to fire events.
export function getHtml(this: ViewerSaveToDriveBubbleElement) {
  return html`<!--_html_template_start_-->
  <dialog id="dialog" @close="${this.onDialogClose_}"
      @focusout="${this.onFocusout_}">
    <div id="header">
      <h2>${this.dialogTitle_}</h2>
      <cr-icon-button id="close" iron-icon="cr:close"
          aria-label="$i18n{propertiesDialogClose}"
          title="$i18n{propertiesDialogClose}"
          @click="${this.onCloseClick_}">
      </cr-icon-button>
    </div>
    <div id="description" .innerHTML="${this.description_}"></div>
    <div id="file-content">
      <cr-icon icon="pdf:pdf-icon" id="pdf-icon"></cr-icon>
      <div id="filename-content">
        <div id="filename">${this.fileName}</div>
        <div id="file-metadata">${this.fileMetadata_}</div>
      </div>
      ${this.isUploading_() ? html`
        <cr-icon-button id="cancel-upload-button"
            iron-icon="pdf:cancel-unfill"
            aria-label="$i18n{saveToDriveDialogCancelUploadButtonLabel}"
            title="$i18n{saveToDriveDialogCancelUploadButtonLabel}">
        </cr-icon-button>
      ` : ''}
    </div>
    <div id="footer">
      ${this.isUploading_() ? html`
        <cr-progress
            .max="${this.bytesToTransfer}"
            .value="${this.bytesTransferred}">
        </cr-progress>
      ` : ''}
    </div>
  </dialog>
<!--_html_template_end_-->`;
}
