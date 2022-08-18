// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert_ts.js';

import {BrowserProxy} from './browser_proxy.js';
import {UserAction} from './cloud_upload.mojom-webui.js';
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

  get proxy() {
    return BrowserProxy.getInstance().handler;
  }

  async connectedCallback() {
    const dialogArgs = chrome.getVariableValue('dialogArguments');
    assert(dialogArgs);
    var args = JSON.parse(dialogArgs);
    assert(args);
    assert(args.path);
    const pathElement = this.$('#path') as HTMLElement;
    pathElement.innerText = `File name: ${args.path}`;

    this.dialog.showModal();
    const cancelButton = this.$('#cancel-button');
    cancelButton.addEventListener('click', () => this.onCancelButtonClick());
    const uploadButton = this.$('#upload-button');
    uploadButton.addEventListener('click', () => this.onUploadButtonClick());

    const {uploadPath} = await this.proxy.getUploadPath();
    const uploadLocationElement = this.$('#upload-location') as HTMLElement;
    uploadLocationElement.innerText = `Upload location: ${uploadPath.path}`;
    uploadLocationElement.toggleAttribute('hidden', false);
  }

  private onCancelButtonClick(): void {
    this.proxy.respondAndClose(UserAction.kCancel);
  }

  private onUploadButtonClick(): void {
    this.proxy.respondAndClose(UserAction.kUpload);
  }
}

customElements.define('cloud-upload-dialog', CloudUploadDialogElement);
