// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {assert} from 'chrome://resources/js/assert_ts.js';

import {BaseSetupPageElement, CANCEL_SETUP_EVENT} from './base_setup_page.js';
import {UserAction} from './cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from './cloud_upload_browser_proxy.js';
import {getTemplate} from './upload_page.html.js';

export enum UploadType {
  ONE_DRIVE = 0,
  DRIVE = 1,
}

/**
 * The UploadPageElement represents the setup page that prompts the user to
 * upload the file to their selected cloud provider.
 */
export class UploadPageElement extends BaseSetupPageElement {
  /** The cloud provider to upload the file to. */
  private uploadType: UploadType|undefined;
  /** The name of the file to upload. */
  private fileName: string;

  constructor() {
    super();
    this.fileName = '';
    this.processDialogArgs();
  }

  private get proxy(): CloudUploadBrowserProxy {
    return CloudUploadBrowserProxy.getInstance();
  }

  /**
   * Initialises the class members based off the given dialog arguments.
   */
  private processDialogArgs(): void {
    try {
      const dialogArgs = this.proxy.getDialogArguments();
      assert(dialogArgs);
      const args = JSON.parse(dialogArgs);
      assert(args);
      assert(args.fileName);
      assert(args.uploadType);
      this.fileName = args.fileName;
      switch (args.uploadType) {
        case 'OneDrive':
          this.uploadType = UploadType.ONE_DRIVE;
          break;
        case 'Drive':
          this.uploadType = UploadType.DRIVE;
          break;
      }
    } catch (e) {
      // TODO(b/243095484) Define expected behavior.
      console.error(`Unable to get dialog arguments . Error: ${e}.`);
    }
  }

  /**
   * Initialises the page specific content inside the page.
   */
  connectedCallback(): void {
    this.innerHTML = getTemplate() as string;
    const titleElement = this.querySelector('#title')! as HTMLElement;
    const uploadMessageElement =
        this.querySelector('#upload-message')! as HTMLElement;
    const fileNameElement = this.querySelector('#file-name')! as HTMLElement;
    const uploadButton = this.querySelector('.action-button')! as HTMLElement;
    const cancelButton = this.querySelector('.cancel-button') as HTMLElement;

    switch (this.uploadType) {
      case UploadType.ONE_DRIVE:
        titleElement.innerText = 'Upload to OneDrive';
        uploadMessageElement.innerText =
            'Upload your file to OneDrive to open with Office.';
        uploadButton.innerText = 'Upload to OneDrive';
        break;
      case UploadType.DRIVE:
        titleElement.innerText = 'Upload to Drive';
        uploadMessageElement.innerText =
            'Upload your file to Google Drive to open with Google Docs.';
        uploadButton.innerText = 'Upload to Drive';
        break;
    }
    fileNameElement.innerText = `File name: ${this.fileName}`;

    uploadButton.addEventListener('click', () => this.onUploadButtonClick());
    cancelButton.addEventListener('click', () => this.onCancelButtonClick());
  }

  private onUploadButtonClick(): void {
    this.proxy.handler.respondAndClose(UserAction.kUpload);
  }

  private onCancelButtonClick(): void {
    this.dispatchEvent(
        new CustomEvent(CANCEL_SETUP_EVENT, {bubbles: true, composed: true}));
  }
}

customElements.define('upload-page', UploadPageElement);
