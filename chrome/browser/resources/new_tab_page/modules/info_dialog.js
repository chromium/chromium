// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * Info dialog that can be populated with custom text via slotting.
 * @polymer
 */
export class InfoDialogElement extends PolymerElement {
  static get is() {
    return 'ntp-info-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  showModal() {
    this.$.dialog.showModal();
  }

  /** @private */
  onCloseClick_() {
    this.$.dialog.close();
  }
}

customElements.define(InfoDialogElement.is, InfoDialogElement);
