// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-disconnect-drive-confirmation-dialog' is a wrapper of
 * <cr-dialog>.
 */
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import '../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './google_drive_confirmation_dialog.html.js';

interface SettingsDriveConfirmationDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

class SettingsDriveConfirmationDialogElement extends PolymerElement {
  static get is() {
    return 'settings-drive-confirmation-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      actionButtonText: String,
      cancelButtonText: String,
      titleText: String,
      bodyText: String,
    };
  }

  /**
   * Keeps track of whether the user accepts the action of the dialog.
   */
  private accept_: boolean;

  constructor() {
    super();
    this.accept_ = true;
  }

  /**
   * When the cancel button is pressed, cancel the dialog.
   */
  private onCancelClick_(): void {
    this.$.dialog.cancel();
  }

  /**
   * When the action button is pressed, close the dialog.
   */
  private onActionClick_(): void {
    this.$.dialog.close();
  }

  /**
   * When the dialog is cancelled ensure the `accept_` is false.
   */
  private onDialogCancel_(): void {
    this.accept_ = false;
  }

  /**
   * When the dialog is closed (either through cancellation or acceptance) send
   * a custom event to the enclosing container.
   */
  private onDialogClose_(e: Event): void {
    e.stopPropagation();

    const closeEvent = new CustomEvent(
        'close',
        {bubbles: true, composed: true, detail: {accept: this.accept_}});
    this.dispatchEvent(closeEvent);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-drive-confirmation-dialog':
        SettingsDriveConfirmationDialogElement;
  }
}

customElements.define(
    SettingsDriveConfirmationDialogElement.is,
    SettingsDriveConfirmationDialogElement);
