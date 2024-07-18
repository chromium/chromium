// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './info_dialog.css.js';
import {getHtml} from './info_dialog.html.js';

export interface InfoDialogElement {
  $: {
    closeButton: HTMLElement,
    dialog: CrDialogElement,
  };
}

/** Info dialog that can be populated with custom text via slotting. */
export class InfoDialogElement extends CrLitElement {
  static get is() {
    return 'ntp-info-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      showOnAttach: {type: Boolean},
    };
  }

  showOnAttach: boolean = false;

  showModal() {
    this.$.dialog.showModal();
  }

  protected onCloseClick_() {
    this.$.dialog.close();
  }
}

customElements.define(InfoDialogElement.is, InfoDialogElement);
