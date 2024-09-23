// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This dialog explains and warns users of the expected outcome
 * when disabling peripheral data access setup.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';

import {getTemplate} from './peripheral_data_access_protection_dialog.html.js';

const SettingsPeripheralDataAccessProtectionDialogElementBase =
    PrefsMixin(PolymerElement);

class SettingsPeripheralDataAccessProtectionDialogElement extends
    SettingsPeripheralDataAccessProtectionDialogElementBase {
  static get is() {
    return 'settings-peripheral-data-access-protection-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefName: {
        type: String,
      },
    };
  }

  prefName: string;

  /**
   * Closes the warning dialog and transitions to the disabling dialog.
   */
  private onDisableClicked_(): void {
    // Send the new state immediately, this will also toggle the underlying
    // setting-toggle-button associated with this pref.
    this.setPrefValue(this.prefName, true);
    this.getWarningDialog_().close();
  }

  private onCancelButtonClicked_(): void {
    this.getWarningDialog_().close();
  }

  private getWarningDialog_(): CrDialogElement {
    return castExists(
        this.shadowRoot!.querySelector<CrDialogElement>('#warningDialog'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsPeripheralDataAccessProtectionDialogElement.is]:
        SettingsPeripheralDataAccessProtectionDialogElement;
  }
}

customElements.define(
    SettingsPeripheralDataAccessProtectionDialogElement.is,
    SettingsPeripheralDataAccessProtectionDialogElement);
