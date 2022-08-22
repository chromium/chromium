// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert_ts.js';

import {UserAction} from './cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from './cloud_upload_browser_proxy.js';
import {getTemplate} from './cloud_upload_dialog.html.js';

/**
 * @fileoverview
 * 'cloud-upload' defines the UI for the "Upload to cloud" workflow.
 */

export class CloudUploadElement extends HTMLElement {
  constructor() {
    super();
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as string;
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
    const cancelButton = this.$('#cancel-button');
    cancelButton.addEventListener('click', () => this.onCancelButtonClick());
    const uploadButton = this.$('#upload-button');
    uploadButton.addEventListener('click', () => this.onUploadButtonClick());

    let fileName = '';
    try {
      const dialogArgs = this.proxy.getDialogArguments();
      assert(dialogArgs);
      const args = JSON.parse(dialogArgs);
      assert(args);
      assert(args.fileName);
      fileName = args.fileName;
    } catch (e) {
      // TODO(b/243095484) Define expected behavior.
      console.error(`Unable to get dialog arguments . Error: ${e}.`);
    }
    this.$('#path').innerText = `File name: ${fileName}`;

    let destinationPath = '';
    try {
      const {uploadPath} = await this.proxy.handler.getUploadPath();
      assert(uploadPath.path);
      destinationPath = uploadPath.path;
    } catch (e) {
      // TODO(b/243095484) Define expected behavior.
      console.error(`Unable to get upload path. Error: ${e}.`);
    }
    const uploadLocationElement = this.$('#upload-location');
    uploadLocationElement.innerText = `Upload location: ${destinationPath}`;
    uploadLocationElement.toggleAttribute('hidden', false);
  }

  private onCancelButtonClick(): void {
    this.proxy.handler.respondAndClose(UserAction.kCancel);
  }

  private onUploadButtonClick(): void {
    this.proxy.handler.respondAndClose(UserAction.kUpload);
  }
}

customElements.define('cloud-upload', CloudUploadElement);
