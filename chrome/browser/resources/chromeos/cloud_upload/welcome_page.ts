// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {BaseSetupPageElement, CANCEL_SETUP_EVENT, NEXT_PAGE_EVENT} from './base_setup_page.js';
import {getTemplate} from './welcome_page.html.js';

/**
 * The WelcomePageElement represents the first page in the setup flow.
 */
export class WelcomePageElement extends BaseSetupPageElement {
  private isZeroStep = false;

  constructor() {
    super();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.innerHTML = getTemplate();

    const description = this.querySelector('#description') as HTMLElement;
    if (this.isZeroStep) {
      description.innerText = 'Files will move to Microsoft OneDrive when' +
          'opening in Microsoft 365';
    }

    const actionButton = this.querySelector('.action-button') as HTMLElement;
    actionButton.addEventListener('click', this.onActionButtonClick);
    if (this.isZeroStep) {
      actionButton.innerText = 'Set up';
    }

    const cancelButton = this.querySelector('.cancel-button') as HTMLElement;
    cancelButton.addEventListener('click', this.onCancelButtonClick);
  }

  setZeroStep() {
    this.isZeroStep = true;
  }

  private onActionButtonClick() {
    this.dispatchEvent(
        new CustomEvent(NEXT_PAGE_EVENT, {bubbles: true, composed: true}));
  }

  private onCancelButtonClick() {
    this.dispatchEvent(
        new CustomEvent(CANCEL_SETUP_EVENT, {bubbles: true, composed: true}));
  }
}

customElements.define('welcome-page', WelcomePageElement);
