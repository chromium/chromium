// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {BrowserProxy} from './browser_proxy.js';
import {getTemplate} from './cloud_upload_dialog.html.js';

/**
 * @fileoverview
 * 'cloud-upload-dialog' defines the UI for the "Upload to cloud" workflow.
 */

class CloudUploadDialogElement extends HTMLElement {
  constructor() {
    super();
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as string;
    const fragment = template.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);
  }

  $<T extends Element>(query: string): T {
    return this.shadowRoot!.querySelector(query)!;
  }

  get dialog(): CrDialogElement {
    return this.$('cr-dialog') as CrDialogElement;
  }

  connectedCallback(): void {
    this.dialog.showModal();
    const cancelButton = this.$('#cancel-button');
    cancelButton.addEventListener('click', () => this.onCancelButtonClick());
    const uploadButton = this.$('#upload-button');
    uploadButton.addEventListener('click', () => this.onUploadButtonClick());
  }

  private onCancelButtonClick(): void {
    chrome.send('dialogClose');
  }

  private async onUploadButtonClick() {
    const proxy = BrowserProxy.getInstance().handler;
    const {uploadPath} = await proxy.getUploadPath();
    const uploadLocationElement = this.$('#upload-location') as HTMLElement;
    uploadLocationElement.innerText = `Upload location: ${uploadPath.path}`;
    uploadLocationElement.toggleAttribute('hidden', false);
  }
}

customElements.define('cloud-upload-dialog', CloudUploadDialogElement);
