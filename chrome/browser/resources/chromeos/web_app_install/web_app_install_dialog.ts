// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './strings.m.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {assert, assertInstanceof} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getTemplate} from './web_app_install_dialog.html.js';

/**
 * @fileoverview
 * 'web-app-install-dialog' defines the UI for the ChromeOS web app install
 * dialog.
 */

class WebAppInstallDialogElement extends HTMLElement {
  static get is() {
    return 'web-app-install-dialog';
  }

  static get template() {
    return getTemplate();
  }

  constructor() {
    super();
    const template = document.createElement('template');
    template.innerHTML = WebAppInstallDialogElement.template as string;
    const fragment = template.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);
  }

  $<T extends Element>(query: string): T {
    return this.shadowRoot!.querySelector(query)!;
  }

  connectedCallback(): void {
    const cancelButton = this.$<CrButtonElement>('.cancel-button');
    assert(cancelButton);
    cancelButton.addEventListener('click', this.onCancelButtonClick.bind(this));

    const installButton = this.$<CrButtonElement>('.install-button')!;
    assert(installButton);
    installButton.addEventListener(
        'click', this.onInstallButtonClick.bind(this));
  }

  private onCancelButtonClick(): void {
    chrome.send('dialogClose');
  }

  private async onInstallButtonClick(event: MouseEvent) {
    assertInstanceof(event.target, CrButtonElement);
    const installButton: CrButtonElement = event.target;

    installButton.textContent = loadTimeData.getString('installing');
    installButton.classList.replace('install', 'installing');
    installButton.disabled = true;

    // Keep the installing state shown for at least 2 seconds to give the
    // impression that the PWA is being installed.
    // TODO(crbug.com/1488697): Call out to actually install the PWA.
    await new Promise(resolve => setTimeout(resolve, 2000));

    // TODO(crbug.com/1488697): Show an "Open app" button instead of
    // "Installed".
    installButton.textContent = 'Installed';
    installButton.classList.replace('installing', 'installed');
  }
}

customElements.define(
    WebAppInstallDialogElement.is, WebAppInstallDialogElement);
