// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {WaffleBrowserProxy} from './browser_proxy.js';

export class WaffleAppElement extends PolymerElement {
  static get is() {
    return 'waffle-app';
  }

  static get template() {
    return getTemplate();
  }

  override connectedCallback() {
    super.connectedCallback();

    afterNextRender(this, () => {
      WaffleBrowserProxy.getInstance().handler.displayDialog();
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'waffle-app': WaffleAppElement;
  }
}

customElements.define(WaffleAppElement.is, WaffleAppElement);
