// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'git local-data-recovery-dialog' is a confirmation dialog that pops up
 * when the user chooses to disable local data recovery.
 *
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/js/assert.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {ConfigureResult} from 'chrome://resources/mojo/chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LockStateMixin} from '../lock_state_mixin.js';

import {getTemplate} from './local_data_recovery_dialog.html.js';

const LocalDataRecoveryDialogElementBase = LockStateMixin(PolymerElement);

interface LocalDataRecoveryDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

class LocalDataRecoveryDialogElement extends
    LocalDataRecoveryDialogElementBase {
  static get is() {
    return 'local-data-recovery-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Authentication token provided by settings-people-page.
       */
      authToken: {
        type: String,
        notify: true,
      },
    };
  }

  authToken: string|undefined;

  constructor() {
    super();
  }

  override connectedCallback(): void {
    super.connectedCallback();
    this.$.dialog.showModal();
  }

  close(): void {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
  }

  private onClose_(): void {
    this.close();
  }

  private onCancelClick_(): void {
    this.close();
  }

  private async onDisableClick_(): Promise<void> {
    try {
      if (typeof this.authToken !== 'string') {
        console.error('Recovery changed with expired token.');
        return;
      }

      const {result} =
          await this.recoveryFactorEditor.configure(this.authToken, false);
      if (result !== ConfigureResult.kSuccess) {
        console.error('RecoveryFactorEditor::Configure failed:', result);
      }
    } finally {
      this.close();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [LocalDataRecoveryDialogElement.is]: LocalDataRecoveryDialogElement;
  }
}

customElements.define(
    LocalDataRecoveryDialogElement.is, LocalDataRecoveryDialogElement);
