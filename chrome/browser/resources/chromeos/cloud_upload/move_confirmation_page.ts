// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';

import {UserAction} from './cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from './cloud_upload_browser_proxy.js';
import {getTemplate} from './move_confirmation_page.html.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';

export enum CloudProvider {
  GOOGLE_DRIVE,
  ONE_DRIVE,
}

/**
 * The MoveConfirmationPageElement represents the dialog page shown when the
 * user opens a file that needs to be moved first, and they haven't yet decided
 * to always move files.
 */
export class MoveConfirmationPageElement extends HTMLElement {
  private proxy: CloudUploadBrowserProxy =
      CloudUploadBrowserProxy.getInstance();
  private cloudProvider: CloudProvider|undefined;

  constructor() {
    super();

    const shadowRoot = this.attachShadow({mode: 'open'});

    shadowRoot.innerHTML = getTemplate();
    const moveButton = shadowRoot.querySelector<HTMLElement>('.action-button')!;
    const cancelButton =
        shadowRoot.querySelector<HTMLElement>('.cancel-button')!;

    moveButton.addEventListener('click', () => this.onMoveButtonClick());
    cancelButton.addEventListener('click', () => this.onCancelButtonClick());
  }

  private getProviderText(cloudProvider: CloudProvider) {
    if (cloudProvider === CloudProvider.ONE_DRIVE) {
      return {
        name: 'Microsoft OneDrive',
        appName: 'Microsoft 365',
        shortName: 'OneDrive',
      };
    }
    // TODO(b/260141250): Display Slides or Sheets when appropriate instead?
    return {name: 'Google Drive', appName: 'Google Docs', shortName: 'Drive'};
  }

  setCloudProvider(cloudProvider: CloudProvider) {
    this.cloudProvider = cloudProvider;

    const {name, appName, shortName} = this.getProviderText(this.cloudProvider);
    this.shadowRoot!.getElementById('provider-name')!.innerText = name;
    this.shadowRoot!.getElementById('app-name')!.innerText = appName;
    this.shadowRoot!.getElementById('provider-short-name')!.innerText =
        shortName;
  }

  private onMoveButtonClick(): void {
    const checkbox = this.shadowRoot!.querySelector<CrCheckboxElement>(
        '#always-move-checkbox')!;
    this.proxy.handler.setAlwaysMoveOfficeFiles(checkbox.checked);

    if (this.cloudProvider === CloudProvider.ONE_DRIVE) {
      this.proxy.handler.respondAndClose(UserAction.kUploadToOneDrive);
    } else {
      this.proxy.handler.respondAndClose(UserAction.kUploadToGoogleDrive);
    }
  }

  private onCancelButtonClick(): void {
    this.proxy.handler.respondAndClose(UserAction.kCancel);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'move-confirmation-page': MoveConfirmationPageElement;
  }
}

customElements.define('move-confirmation-page', MoveConfirmationPageElement);