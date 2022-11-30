// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './info_dialog.html.js';

export interface InfoDialogElement {
  $: {
    closeButton: HTMLElement,
    dialog: CrDialogElement,
  };
}

/** Info dialog that can be populated with custom text via slotting. */
export class InfoDialogElement extends PolymerElement {
  static get is() {
    return 'ntp-info-dialog';
  }

  static get template() {
    return getTemplate();
  }

  showModal() {
    this.$.dialog.showModal();
  }

  private onCloseClick_() {
    this.$.dialog.close();
  }
}

customElements.define(InfoDialogElement.is, InfoDialogElement);
