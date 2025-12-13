// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SaveToDriveState} from '../constants.js';

import type {ViewerSaveToDriveBubbleElement} from './viewer_save_to_drive_bubble.js';

// TODO(crbug.com/427451594): Hook up the buttons to fire events.
export function getHtml(this: ViewerSaveToDriveBubbleElement) {
  // clang-format off
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
      <div id="description" .innerHTML="${this.description_}"
          ?hidden="${this.isSaveToDriveState_(SaveToDriveState.UPLOADING)}">
      </div>
      <div id="file-content">
        <cr-icon icon="pdf:pdf-icon" id="pdf-icon"></cr-icon>
        <div id="filename-content">
          <div id="filename">${this.getFileName_()}</div>
          <div id="file-metadata">${this.getMetadata_()}</div>
        </div>
        ${this.isSaveToDriveState_(SaveToDriveState.UPLOADING) ? html`
          <cr-icon-button id="cancel-upload-button"
              iron-icon="pdf:cancel-unfill"
              aria-label="$i18n{saveToDriveDialogCancelUploadButtonLabel}"
              title="$i18n{saveToDriveDialogCancelUploadButtonLabel}"
              @click="${this.onRequestButtonClick_}">
          </cr-icon-button>
        ` : ''}
      </div>
      <div id="footer">
        ${this.isSaveToDriveState_(SaveToDriveState.UPLOADING) ? html`
          <cr-progress
              .max="${this.getFileSizeBytes_()}"
              .value="${this.getUploadedBytes_()}">
          </cr-progress>
        ` : ''}
        ${this.isSaveToDriveState_(SaveToDriveState.STORAGE_FULL_ERROR) ? html`
          <cr-button id="manage-storage-button" role="link"
              @click="${this.onRequestButtonClick_}">
            $i18n{saveToDriveDialogManageStorageButtonLabel}
          </cr-button>
        ` : ''}
        ${this.isSaveToDriveState_(SaveToDriveState.SUCCESS) ? html`
          <cr-button id="open-in-drive-button" class="action-button" role="link"
              @click="${this.onRequestButtonClick_}">
            $i18n{saveToDriveDialogOpenInDriveButtonLabel}
          </cr-button>
        ` : ''}
        ${this.isSaveToDriveState_(SaveToDriveState.CONNECTION_ERROR) ||
          this.isSaveToDriveState_(SaveToDriveState.SESSION_TIMEOUT_ERROR) ?
        html`
          <cr-button id="retry-button" class="action-button"
              @click="${this.onRequestButtonClick_}">
            $i18n{saveToDriveDialogRetryButtonLabel}
          </cr-button>
        ` : ''}
      </div>
    </dialog>
  <!--_html_template_end_-->`;
  // clang-format on
}
