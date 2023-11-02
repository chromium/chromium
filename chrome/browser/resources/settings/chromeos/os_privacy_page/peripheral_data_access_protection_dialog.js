// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This dialog explains and warns users of the expected outcome
 * when disabling peripheral data access setup.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import '../../settings_shared.css.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {PrefsBehaviorInterface}
 */
const SettingsPeripheralDataAccessProtectionDialogElementBase =
    mixinBehaviors([PrefsBehavior], PolymerElement);

/** @polymer */
class SettingsPeripheralDataAccessProtectionDialogElement extends
    SettingsPeripheralDataAccessProtectionDialogElementBase {
  static get is() {
    return 'settings-peripheral-data-access-protection-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      prefName: {
        type: String,
      },
    };
  }

  /**
   * Closes the warning dialog and transitions to the disabling dialog.
   * @private
   */
  onDisableClicked_() {
    // Send the new state immediately, this will also toggle the underlying
    // setting-toggle-button associated with this pref.
    this.setPrefValue(this.prefName, true);
    this.shadowRoot.querySelector('#warningDialog').close();
  }

  /** @private */
  onCancelButtonClicked_() {
    this.shadowRoot.querySelector('#warningDialog').close();
  }
}

customElements.define(
    SettingsPeripheralDataAccessProtectionDialogElement.is,
    SettingsPeripheralDataAccessProtectionDialogElement);
