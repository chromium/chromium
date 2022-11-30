// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert_ts.js';

import {UserAction} from './cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from './cloud_upload_browser_proxy.js';
import {getTemplate} from './cloud_upload_dialog.html.js';

export enum UploadType {
  ONE_DRIVE = 0,
  DRIVE = 1,
}

/**
 * @fileoverview
 * 'cloud-upload' defines the UI for the "Upload to cloud" workflow.
 */

export class CloudUploadElement extends HTMLElement {
  uploadType: UploadType|undefined;
  fileName: string;

  constructor() {
    super();
    this.fileName = '';
    this.processDialogArgs();
    const template = this.createTemplate();
    const fragment = template.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);
  }

  $<T extends HTMLElement>(query: string): T {
    return this.shadowRoot!.querySelector(query)!;
  }

  get dialog(): CrDialogElement {
    return this.$('cr-dialog') as CrDialogElement;
  }

  get proxy() {
    return CloudUploadBrowserProxy.getInstance();
  }

  async connectedCallback() {
    const cancelButton = this.$('#cancel-button')! as HTMLElement;
    const uploadButton = this.$('#upload-button')! as HTMLElement;
    cancelButton.addEventListener('click', () => this.onCancelButtonClick());
    uploadButton.addEventListener('click', () => this.onUploadButtonClick());
  }

  processDialogArgs() {
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

  createTemplate(): HTMLTemplateElement {
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as string;
    const fragment = template.content;
    const titleElement =
        fragment.querySelector('div[slot="title"]')! as HTMLElement;
    const uploadMessageElement =
        fragment.querySelector('#upload-message')! as HTMLElement;
    const fileNameElement =
        fragment.querySelector('#file-name')! as HTMLElement;
    const uploadButton =
        fragment.querySelector('#upload-button')! as HTMLElement;

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

    return template;
  }

  private onCancelButtonClick(): void {
    this.proxy.handler.respondAndClose(UserAction.kCancel);
  }

  private onUploadButtonClick(): void {
    this.proxy.handler.respondAndClose(UserAction.kUpload);
  }
}

customElements.define('cloud-upload', CloudUploadElement);
