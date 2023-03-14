// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {getTemplate} from './deprecated_apps_link.html.js';

export class DeprecatedAppsLinkElement extends PolymerElement {
  static get is() {
    return 'deprecated-apps-link';
  }

  static get properties() {
    return {
      deprecationLinkString: String,
    };
  }

  deprecationLinkString: string = '';
  display: string = 'none';

  static get template() {
    return getTemplate();
  }

  constructor() {
    super();

    BrowserProxy.getInstance().handler.getDeprecationLinkString().then(
        result => {
          this.display = result.linkString === '' ? 'none' : 'inline-flex';
          this.deprecationLinkString = result.linkString;
        });
  }

  private linkClicked_() {
    BrowserProxy.getInstance().handler.launchDeprecatedAppDialog();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'deprecated-apps-link': DeprecatedAppsLinkElement;
  }
}

customElements.define(DeprecatedAppsLinkElement.is, DeprecatedAppsLinkElement);
