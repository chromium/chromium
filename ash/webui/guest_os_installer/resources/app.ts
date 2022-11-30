// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {BrowserProxy} from './browser_proxy.js';

class GuestOsInstallerElement extends PolymerElement {
  static get is() {
    return 'guest-os-installer';
  }

  static get template() {
    return getTemplate();
  }

  private listenerIds_: number[] = [];

  override connectedCallback() {
    super.connectedCallback();
    this.listenerIds_ = [];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    const callbackRouter = BrowserProxy.getInstance().callbackRouter;
    this.listenerIds_.forEach(
        (id: number) => callbackRouter.removeListener(id));
    this.listenerIds_.length = 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'guest-os-installer': GuestOsInstallerElement;
  }
}

customElements.define(GuestOsInstallerElement.is, GuestOsInstallerElement);
