// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_icons.css.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {assertInstanceof} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {BaseSetupPageElement, CANCEL_SETUP_EVENT, NEXT_PAGE_EVENT} from './base_setup_page.js';
import {CloudUploadBrowserProxy} from './cloud_upload_browser_proxy.js';
import {getTemplate} from './office_pwa_install_page.html.js';

/**
 * The OfficePwaInstallPageElement guides the user through installing the
 * Microsoft 365 web app.
 */
export class OfficePwaInstallPageElement extends BaseSetupPageElement {
  override connectedCallback(): void {
    super.connectedCallback();

    this.innerHTML = getTemplate();

    const actionButton = this.querySelector<HTMLElement>('.action-button')!;
    actionButton.addEventListener('click', this.onActionButtonClick.bind(this));

    const cancelButton = this.querySelector<HTMLElement>('.cancel-button')!;
    cancelButton.addEventListener('click', this.onCancelButtonClick.bind(this));
  }

  private async onActionButtonClick(event: MouseEvent) {
    const proxy = CloudUploadBrowserProxy.getInstance();

    assertInstanceof(event.target, CrButtonElement);
    const actionButton: CrButtonElement = event.target;
    actionButton.innerText = loadTimeData.getString('installing');
    actionButton.classList.replace('install', 'installing');
    actionButton.disabled = true;

    const testTime = 30;
    // Keep the installing state shown for a minimum time to give the
    // impression that the web app is being installed.
    const installingTime = 3000;
    const [{installed: install_result}] = await Promise.all([
      proxy.handler.installOfficeWebApp(),
      new Promise(
          resolve =>
              setTimeout(resolve, proxy.isTest() ? testTime : installingTime)),
    ]);

    if (install_result) {
      actionButton.innerText = loadTimeData.getString('installed');
      actionButton.classList.replace('installing', 'installed');

      // Keep the installed state shown for a minimum time before changing
      // pages to give the user feedback that the web app has been installed.
      const installedTime = 2000;
      await new Promise(
          resolve =>
              setTimeout(resolve, proxy.isTest() ? testTime : installedTime));

      this.dispatchEvent(
          new CustomEvent(NEXT_PAGE_EVENT, {bubbles: true, composed: true}));
    } else {
      // TODO(b:251046341): Proper error display.
      actionButton.innerText = loadTimeData.getString('install');
      actionButton.classList.replace('installing', 'install');
      actionButton.disabled = false;
    }
  }

  private onCancelButtonClick() {
    this.dispatchEvent(
        new CustomEvent(CANCEL_SETUP_EVENT, {bubbles: true, composed: true}));
  }
}

customElements.define('office-pwa-install-page', OfficePwaInstallPageElement);
