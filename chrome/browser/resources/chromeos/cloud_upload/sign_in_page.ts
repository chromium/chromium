// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';

import {BaseSetupPageElement, CANCEL_SETUP_EVENT, NEXT_PAGE_EVENT} from './base_setup_page.js';
import {CloudUploadBrowserProxy} from './cloud_upload_browser_proxy.js';
import {getTemplate} from './sign_in_page.html.js';

/**
 * The SignInPageElement represents the page that prompts the user to connect to
 * OneDrive.
 */
export class SignInPageElement extends BaseSetupPageElement {
  private get proxy(): CloudUploadBrowserProxy {
    return CloudUploadBrowserProxy.getInstance();
  }

  /**
   * Initialises the page specific content inside the page.
   */
  override connectedCallback(): void {
    super.connectedCallback();

    this.innerHTML = getTemplate();
    const connectButton = this.querySelector<HTMLElement>('.action-button')!;
    const cancelButton = this.querySelector<HTMLElement>('.cancel-button')!;

    connectButton.addEventListener('click', () => this.onConnectButtonClick());
    cancelButton.addEventListener('click', () => this.onCancelButtonClick());
  }

  async onConnectButtonClick(): Promise<void> {
    const {success: signInSuccess} =
        await this.proxy.handler.signInToOneDrive();
    if (signInSuccess) {
      this.dispatchEvent(
          new CustomEvent(NEXT_PAGE_EVENT, {bubbles: true, composed: true}));
    } else {
      const errorMessage = this.querySelector<HTMLElement>('#error-message')!;
      errorMessage.toggleAttribute('hidden', false);
      // Update top/bottom fade style if the dialog's content overflows.
      const contentElement =
          this.shadowRoot!.querySelector<HTMLElement>('#content')!;
      contentElement.scrollTop = 0;
      this.updateContentFade(contentElement);
    }
  }

  private onCancelButtonClick(): void {
    this.dispatchEvent(
        new CustomEvent(CANCEL_SETUP_EVENT, {bubbles: true, composed: true}));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'sign-in-page': SignInPageElement;
  }
}

customElements.define('sign-in-page', SignInPageElement);
