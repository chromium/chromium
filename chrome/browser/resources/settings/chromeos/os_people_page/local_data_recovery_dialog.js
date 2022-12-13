// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'git local-data-recovery-dialog' is a confirmation dialog that pops up
 * when the user chooses to disable local data recovery.
 *
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/assert.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../settings_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {AuthFactor, FactorObserverInterface, FactorObserverReceiver, ManagementType, RecoveryFactorEditor_ConfigureResult} from 'chrome://resources/mojo/chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-webui.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LockScreenUnlockType, LockStateBehavior, LockStateBehaviorInterface} from './lock_state_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {LockStateBehaviorInterface}
 */
const LocalDataRecoveryDialogElementBase = mixinBehaviors(
    [
      I18nBehavior,
      LockStateBehavior,
    ],
    PolymerElement);

/** @polymer */
class LocalDataRecoveryDialogElement extends
    LocalDataRecoveryDialogElementBase {
  static get is() {
    return 'local-data-recovery-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Authentication token provided by settings-people-page.
       */
      authToken: {
        type: Object,
        notify: true,
      },
    };
  }

  constructor() {
    super();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.$.dialog.showModal();
  }

  close() {
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
  }

  /** @private */
  onClose_() {
    this.close();
  }

  /** @private */
  onCancelTap_() {
    this.close();
  }

  /** @private */
  async onDisableTap_() {
    try {
      if (!this.authToken) {
        console.error('Recovery changed with expired token.');
        return;
      }

      const {result} = await this.recoveryFactorEditor.configure(
          this.authToken.token, false);
      if (result !== RecoveryFactorEditor_ConfigureResult.kSuccess) {
        console.error('RecoveryFactorEditor::Configure failed:', result);
      }
    } finally {
      this.close();
    }
  }
}

customElements.define(
    LocalDataRecoveryDialogElement.is, LocalDataRecoveryDialogElement);
