// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-disconnect-drive-confirmation-dialog' is a wrapper of
 * <cr-dialog>.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './google_drive_disconnect_drive_dialog.html.js';

interface SettingsDisconnectDriveConfirmationDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

class SettingsDisconnectDriveConfirmationDialogElement extends PolymerElement {
  static get is() {
    return 'settings-disconnect-drive-confirmation-dialog';
  }

  static get template() {
    return getTemplate();
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
  private onCancelClick_() {
    this.$.dialog.cancel();
  }

  /**
   * When the disconnect button is pressed, close the dialog.
   */
  private onDisconnectClick_() {
    this.$.dialog.close();
  }

  /**
   * When the dialog is cancelled ensure the `accept_` is false.
   */
  private onDialogCancel_() {
    this.accept_ = false;
  }

  /**
   * When the dialog is closed (either through cancellation or acceptance) send
   * a custom event to the enclosing container.
   */
  private onDialogClose_(e: Event) {
    e.stopPropagation();

    const closeEvent = new CustomEvent(
        'close',
        {bubbles: true, composed: true, detail: {'accept': this.accept_}});
    this.dispatchEvent(closeEvent);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-disconnect-drive-confirmation-dialog':
        SettingsDisconnectDriveConfirmationDialogElement;
  }
}

customElements.define(
    SettingsDisconnectDriveConfirmationDialogElement.is,
    SettingsDisconnectDriveConfirmationDialogElement);
