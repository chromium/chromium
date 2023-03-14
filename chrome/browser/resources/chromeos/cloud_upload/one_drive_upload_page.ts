// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {BaseSetupPageElement} from './base_setup_page.js';
import {UserAction} from './cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from './cloud_upload_browser_proxy.js';
import {getTemplate} from './one_drive_upload_page.html.js';

/**
 * The OneDriveUploadPageElement represents the setup page that prompts the user
 * to upload the file to OneDrive.
 */
export class OneDriveUploadPageElement extends BaseSetupPageElement {
  /** The names of the files to upload. */
  // Commented out to meet current UX specifications.
  // private fileNames: string[] = [];

  /**
    True if the setup flow is being run for the first time. False if the fixup
    flow is being run.
  */
  private firstTimeSetup: boolean = true;

  constructor() {
    super();
  }

  /**
   * Sets the file name to be displayed by this dialog. Can be null if there is
   * no file to upload. Sets whether the setup flow is running for the first
   * time.
   * @param fileName Name of the file to be displayed.
   * @param firstTimeSetup Whether the setup flow is running for the first time.
   */
  setFileNamesAndFirstTimeSetup(_fileNames: string[], firstTimeSetup: boolean) {
    // this.fileNames = fileNames;
    this.firstTimeSetup = firstTimeSetup;
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
    const uploadButton = this.querySelector('.action-button')! as HTMLElement;

    if (this.firstTimeSetup) {
      this.proxy.handler.setOfficeAsDefaultHandler();
    }

    uploadButton.addEventListener('click', () => this.onUploadButtonClick());
  }

  private onUploadButtonClick(): void {
    this.proxy.handler.respondWithUserActionAndClose(
        UserAction.kConfirmOrUploadToOneDrive);
  }
}

customElements.define('upload-page', OneDriveUploadPageElement);
