// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './selection_overlay.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getTemplate} from './lens_overlay_app.html.js';

export interface LensOverlayAppElement {
  $: {
    closeButton: CrButtonElement,
  };
}

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
