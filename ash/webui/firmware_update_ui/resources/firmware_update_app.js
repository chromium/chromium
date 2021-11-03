// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './firmware_shared_css.js';
import './firmware_shared_fonts.js';
import './peripheral_updates_list.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'firmware-update-app' is the main landing page for the firmware
 * update app.
 */
export class FirmwareUpdateAppElement extends PolymerElement {
  static get is() {
    return 'firmware-update-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(FirmwareUpdateAppElement.is, FirmwareUpdateAppElement);