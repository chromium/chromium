// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'peripheral-updates-list' displays a list of available peripheral updates.
 */
export class PeripheralUpdateListElement extends PolymerElement {
  static get is() {
    return 'peripheral-updates-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(
    PeripheralUpdateListElement.is, PeripheralUpdateListElement);
