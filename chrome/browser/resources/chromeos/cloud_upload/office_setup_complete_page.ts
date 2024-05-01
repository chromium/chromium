// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';

import {assert} from 'chrome://resources/js/assert.js';

import {BaseSetupPageElement} from './base_setup_page.js';
import {UserAction} from './cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from './cloud_upload_browser_proxy.js';
import {getTemplate} from './office_setup_complete_page.html.js';

/**
 * The OfficeSetupCompletePageElement represents the page that shows the
 * completed state of the setup flow.
 */
export class OfficeSetupCompletePageElement extends BaseSetupPageElement {
  /**
    True if Microsoft 365 should be set as default handler when this page gets
    displayed.
  */
  private setOfficeAsDefaultHandler: boolean = true;

  constructor() {
    super();
  }

  /**
   * @param setOfficeAsDefaultHandler Whether Microsoft 365 should be set as
   *     default handler when this page gets displayed.
   */
  setDefaultHandlerOnPageShown(setOfficeAsDefaultHandler: boolean) {
    this.setOfficeAsDefaultHandler = setOfficeAsDefaultHandler;
    if (this.isConnected) {
      this.connectedCallback();
    }
  }

  private get proxy(): CloudUploadBrowserProxy {
    return CloudUploadBrowserProxy.getInstance();
  }

  /**
   * Initialises the page specific content inside the page.
   */
  override connectedCallback(): void {
    super.connectedCallback();

    this.innerHTML = getTemplate();
    const uploadButton = this.querySelector<HTMLElement>('.action-button');
    assert(uploadButton);

    if (this.setOfficeAsDefaultHandler) {
      this.proxy.handler.setOfficeAsDefaultHandler();
    }

    uploadButton.addEventListener('click', () => this.onUploadButtonClick());
  }

  private onUploadButtonClick(): void {
    this.proxy.handler.respondWithUserActionAndClose(
        UserAction.kConfirmOrUploadToOneDrive);
  }
}

customElements.define('complete-page', OfficeSetupCompletePageElement);
