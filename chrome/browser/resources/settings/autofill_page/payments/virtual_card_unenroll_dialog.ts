// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-virtual-card-unenroll-dialog' is the dialog that is
 * shown when the action menu button "Remove virtual card" is clicked. It
 * requests user confirmation before unenrolling a card from the virtual card
 * feature.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {getCss as getCrSharedStyleCss} from 'chrome://resources/cr_elements/cr_shared_style_lit.css.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss as getSettingsSharedCss} from '../../settings_shared_lit.css.js';

import {getHtml} from './virtual_card_unenroll_dialog.html.js';

declare global {
  interface HTMLElementEventMap {
    'unenroll-virtual-card': CustomEvent<string>;
  }
}

export interface SettingsVirtualCardUnenrollDialogElement {
  $: {
    dialog: CrDialogElement,
    confirmButton: CrButtonElement,
  };
}

export class SettingsVirtualCardUnenrollDialogElement extends CrLitElement {
  static get is() {
    return 'settings-virtual-card-unenroll-dialog';
  }

  static override get styles() {
    return [
      getCrSharedStyleCss(),
      getSettingsSharedCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The credit card being unenrolled from the virtual cards.
       */
      creditCard: {type: Object},
    };
  }

  accessor creditCard: chrome.autofillPrivate.CreditCardEntry = {};

  close() {
    this.$.dialog.close();
  }

  protected onCancelButtonClick_() {
    this.$.dialog.cancel();
  }

  protected onConfirmButtonClick_() {
    this.fire('unenroll-virtual-card', this.creditCard.guid);
    this.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-virtual-card-unenroll-dialog':
        SettingsVirtualCardUnenrollDialogElement;
  }
}

customElements.define(
    SettingsVirtualCardUnenrollDialogElement.is,
    SettingsVirtualCardUnenrollDialogElement);
