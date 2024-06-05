// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {BrowserProxy} from './browser_proxy.js';
import {getCss} from './deprecated_apps_link.css.js';
import {getHtml} from './deprecated_apps_link.html.js';

export class DeprecatedAppsLinkElement extends CrLitElement {
  static get is() {
    return 'deprecated-apps-link';
  }

  static override get properties() {
    return {
      deprecationLinkString: {type: String},
    };
  }

  deprecationLinkString: string = '';
  display: boolean = false;

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  constructor() {
    super();

    BrowserProxy.getInstance().handler.getDeprecationLinkString().then(
        result => {
          this.display = !!result.linkString && result.linkString.length > 0;
          this.deprecationLinkString = result.linkString;
        });
  }

  protected onLinkClick_() {
    BrowserProxy.getInstance().handler.launchDeprecatedAppDialog();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'deprecated-apps-link': DeprecatedAppsLinkElement;
  }
}

customElements.define(DeprecatedAppsLinkElement.is, DeprecatedAppsLinkElement);
