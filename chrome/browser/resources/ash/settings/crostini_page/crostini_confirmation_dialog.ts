// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-confirmation-dialog' is a wrapper of
 * <cr-dialog> which
 *
 * - shows an accept button and a cancel button (you can customize the label via
 *   props);
 * - The close event has a boolean `e.detail.accepted` indicating whether the
 *   dialog is accepted or not.
 * - The dialog shows itself automatically when it is attached.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './crostini_confirmation_dialog.html.js';

export interface SettingsCrostiniConfirmationDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class SettingsCrostiniConfirmationDialogElement extends PolymerElement {
  static get is() {
    return 'settings-crostini-confirmation-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      acceptButtonText: String,
      cancelButtonText: {
        type: String,
        value: loadTimeData.getString('cancel'),
      },
    };
  }

  private accepted_: boolean;

  constructor() {
    super();

    this.accepted_ = true;
  }

  private onCancelClick_(): void {
    this.$.dialog.cancel();
  }

  private onAcceptClick_(): void {
    this.$.dialog.close();
  }

  private onDialogCancel_(): void {
    this.accepted_ = false;
  }

  private onDialogClose_(e: Event): void {
    e.stopPropagation();

    const closeEvent = new CustomEvent(
        'close',
        {bubbles: true, composed: true, detail: {accepted: this.accepted_}});
    this.dispatchEvent(closeEvent);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-crostini-confirmation-dialog':
        SettingsCrostiniConfirmationDialogElement;
  }
}

customElements.define(
    SettingsCrostiniConfirmationDialogElement.is,
    SettingsCrostiniConfirmationDialogElement);
