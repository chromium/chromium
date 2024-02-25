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
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../settings_shared.css.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './virtual_card_unenroll_dialog.html.js';

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

export class SettingsVirtualCardUnenrollDialogElement extends PolymerElement {
  static get is() {
    return 'settings-virtual-card-unenroll-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The credit card being unenrolled from the virtual cards.
       */
      creditCard: Object,
    };
  }

  creditCard: chrome.autofillPrivate.CreditCardEntry;
  private title_: string;
  private label_: string;

  close() {
    this.$.dialog.close();
  }

  private onCancelButtonClick_() {
    this.$.dialog.cancel();
  }

  private onConfirmButtonClick_() {
    this.dispatchEvent(new CustomEvent(
        'unenroll-virtual-card',
        {bubbles: true, composed: true, detail: this.creditCard.guid!}));
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
