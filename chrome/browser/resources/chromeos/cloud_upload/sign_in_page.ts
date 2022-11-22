// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {BaseSetupPageElement, CANCEL_SETUP_EVENT, NEXT_PAGE_EVENT} from './base_setup_page.js';
import {getTemplate} from './sign_in_page.html.js';

/**
 * The SignInPageElement represents the page that prompts the user to connect to
 * OneDrive.
 */
export class SignInPageElement extends BaseSetupPageElement {
  /**
   * Initialises the page specific content inside the page.
   */
  connectedCallback(): void {
    this.innerHTML = getTemplate();
    const connectButton = this.querySelector<HTMLElement>('.action-button')!;
    const cancelButton = this.querySelector<HTMLElement>('.cancel-button')!;

    connectButton.addEventListener('click', () => this.onConnectButtonClick());
    cancelButton.addEventListener('click', () => this.onCancelButtonClick());
  }

  private onConnectButtonClick(): void {
    this.dispatchEvent(
        new CustomEvent(NEXT_PAGE_EVENT, {bubbles: true, composed: true}));
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
