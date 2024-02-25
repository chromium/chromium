// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import {UserAction} from './cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from './cloud_upload_browser_proxy.js';
import {getTemplate} from './connect_onedrive.html.js';

/**
 * The ConnectOneDriveElement represents the dialog shown when the user selects
 * OneDrive from the Files app Services menu, without going through the normal
 * setup flow.
 */
export class ConnectOneDriveElement extends HTMLElement {
  private proxy: CloudUploadBrowserProxy =
      CloudUploadBrowserProxy.getInstance();

  // Save reference to listener so it can be removed from the document in
  // disconnectedCallback().
  private boundKeyDownListener_: (e: KeyboardEvent) => void;


  constructor() {
    super();

    const shadowRoot = this.attachShadow({mode: 'open'});

    shadowRoot.innerHTML = getTemplate();
    const connectButton = this.$('.action-button')!;
    const closeButton = this.$('.cancel-button')!;

    connectButton.addEventListener('click', () => this.onConnectButtonClick());
    closeButton.addEventListener('click', () => this.onCloseButtonClick());
    this.boundKeyDownListener_ = this.onKeyDown.bind(this);
  }

  connectedCallback(): void {
    document.addEventListener('keydown', this.boundKeyDownListener_);
  }

  disconnectedCallback(): void {
    document.removeEventListener('keydown', this.boundKeyDownListener_);
  }

  $<T extends HTMLElement>(query: string): T {
    return this.shadowRoot!.querySelector(query)!;
  }

  private async onConnectButtonClick(): Promise<void> {
    const {success: signInSuccess} =
        await this.proxy.handler.signInToOneDrive();

    if (signInSuccess) {
      // Change to success page.
      this.shadowRoot!.querySelector<SVGUseElement>('#install')!.setAttribute(
          'visibility', 'hidden');
      this.shadowRoot!.querySelector<SVGUseElement>('#success')!.setAttribute(
          'visibility', 'visible');
      this.$('#title').innerText =
          loadTimeData.getString('oneDriveConnectedTitle');
      this.$('#body-text').innerText =
          loadTimeData.getString('oneDriveConnectedBodyText');
      this.$('.action-button')!.remove();
    } else {
      this.$('#error-message').toggleAttribute('hidden', false);
    }
  }

  private onCloseButtonClick(): void {
    this.proxy.handler.respondWithUserActionAndClose(UserAction.kCancel);
  }

  private onKeyDown(e: KeyboardEvent) {
    if (e.key === 'Escape') {
      // Handle Escape as a "cancel".
      e.stopImmediatePropagation();
      e.preventDefault();
      this.onCloseButtonClick();
      return;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'connect-onedrive': ConnectOneDriveElement;
  }
}

customElements.define('connect-onedrive', ConnectOneDriveElement);
