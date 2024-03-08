// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';

export class LensOverlayAppElement extends PolymerElement {
  static get is() {
    return 'lens-overlay-app';
  }

  static get template() {
    return getTemplate();
  }

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  private onCloseButtonClick_() {
    this.browserProxy_.handler.closeRequestedByOverlay();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'lens-overlay-app': LensOverlayAppElement;
  }
}

customElements.define(LensOverlayAppElement.is, LensOverlayAppElement);
