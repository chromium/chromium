// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {ExtendedUpdatesBrowserProxy} from './extended_updates_browser_proxy.js';

export class ExtendedUpdatesAppElement extends PolymerElement {
  static get is() {
    return 'extended-updates-app';
  }

  static get template() {
    return getTemplate();
  }

  private extendedUpdatesBrowserProxy_: ExtendedUpdatesBrowserProxy;

  constructor() {
    super();

    this.extendedUpdatesBrowserProxy_ =
        ExtendedUpdatesBrowserProxy.getInstance();
  }

  private onOptInButtonClick_(): void {
    this.extendedUpdatesBrowserProxy_.optInToExtendedUpdates();
  }

  private onCloseButtonClick_(): void {
    return;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extended-updates-app': ExtendedUpdatesAppElement;
  }
}

customElements.define(ExtendedUpdatesAppElement.is, ExtendedUpdatesAppElement);
