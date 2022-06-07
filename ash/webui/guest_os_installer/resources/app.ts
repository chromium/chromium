// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';

class GuestOsInstallerApp extends PolymerElement {
  static get is() {
    return 'guest-os-installer-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  listenerIds_: number[] = [];

  override connectedCallback() {
    this.listenerIds_ = [];
    super.connectedCallback();
  }

  override disconnectedCallback() {
    const callbackRouter = BrowserProxy.getInstance().callbackRouter;
    this.listenerIds_.forEach(
        (id: number) => callbackRouter.removeListener(id));
    super.disconnectedCallback();
  }
}

customElements.define('guest-os-installer-app', GuestOsInstallerApp);
