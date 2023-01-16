// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {assert} from 'chrome://resources/js/assert_ts.js';

import {UserAction} from './cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from './cloud_upload_browser_proxy.js';
import {getTemplate} from './drive_upload_page.html.js';

/**
 * The DriveUploadPageElement represents the setup page the user sees after
 * choosing Docs/Sheets/Slides.
 */
export class DriveUploadPageElement extends HTMLElement {
  private proxy: CloudUploadBrowserProxy =
      CloudUploadBrowserProxy.getInstance();

  constructor() {
    super();
    const shadowRoot = this.attachShadow({mode: 'open'});

    shadowRoot.innerHTML = getTemplate();
    const uploadButton =
        shadowRoot.querySelector<HTMLElement>('.action-button');
    const cancelButton =
        shadowRoot.querySelector<HTMLElement>('.cancel-button');
    assert(uploadButton);
    assert(cancelButton);

    uploadButton.addEventListener('click', () => this.onContinueButtonClick());
    cancelButton.addEventListener('click', () => this.onCancelButtonClick());
  }

  private onContinueButtonClick(): void {
    this.proxy.handler.respondWithUserActionAndClose(
        UserAction.kConfirmOrUploadToGoogleDrive);
  }

  private onCancelButtonClick(): void {
    this.proxy.handler.respondWithUserActionAndClose(UserAction.kCancel);
  }
}

customElements.define('drive-upload-page', DriveUploadPageElement);