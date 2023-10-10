// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
}

customElements.define(
    WebAppInstallDialogElement.is, WebAppInstallDialogElement);
