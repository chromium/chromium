// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import {BaseSetupPageElement, CANCEL_SETUP_EVENT, NEXT_PAGE_EVENT} from './base_setup_page.js';
import {getTemplate} from './welcome_page.html.js';

/**
 * The WelcomePageElement represents the first page in the setup flow.
 */
export class WelcomePageElement extends BaseSetupPageElement {
  private isOfficeWebAppInstalled = false;
  private isOdfsMounted = false;

  constructor() {
    super();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.innerHTML = getTemplate();

    const description = this.querySelector<HTMLElement>('#description')!;
    const actionButton = this.querySelector<HTMLElement>('.action-button')!;
    const cancelButton = this.querySelector<HTMLElement>('.cancel-button')!;

    if (this.isOfficeWebAppInstalled && this.isOdfsMounted) {
      description.innerText = loadTimeData.getString('welcomeMoveFiles');
      actionButton.innerText = loadTimeData.getString('welcomeSetUp');
    } else {
      const ul = document.createElement('ul');
      if (!this.isOfficeWebAppInstalled) {
        const installOfficeWebAppElement = document.createElement('li');
        installOfficeWebAppElement.innerText =
            loadTimeData.getString('welcomeInstallOfficeWebApp');
        ul.appendChild(installOfficeWebAppElement);
      }
      if (!this.isOdfsMounted) {
        const installOdfsElement = document.createElement('li');
        installOdfsElement.innerText =
            loadTimeData.getString('welcomeInstallOdfs');
        ul.appendChild(installOdfsElement);
      }
      const moveFilesElement = document.createElement('li');
      moveFilesElement.innerText = loadTimeData.getString('welcomeMoveFiles');
      ul.appendChild(moveFilesElement);
      description.appendChild(ul);
      actionButton.innerText = loadTimeData.getString('welcomeGetStarted');
    }

    actionButton.addEventListener('click', this.onActionButtonClick);
    cancelButton.addEventListener('click', this.onCancelButtonClick);
  }

  setInstalled(isOfficeWebAppInstalled: boolean, isOdfsMounted: boolean) {
    this.isOfficeWebAppInstalled = isOfficeWebAppInstalled;
    this.isOdfsMounted = isOdfsMounted;
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
