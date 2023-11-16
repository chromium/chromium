// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 *   @fileoverview
 *   Material design button that shows Android phone icon and displays text to
 *   use quick start.
 *
 *   Example:
 *     <quick-start-entry-point
 *       quickStartTextkey="welcomeScreenQuickStart">
 *     </quick-start-entry-point>
 *
 *   Attributes:
 *     'quickStartTextkey' - ID of localized string to be used as button text.
 */

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeI18nBehavior} from './behaviors/oobe_i18n_behavior.js';
import {OobeModalDialog} from './dialogs/oobe_modal_dialog.js';

/**
 * @constructor
 * @extends {PolymerElement}
 */
const QuickStartEntryPointBase =
    mixinBehaviors([OobeI18nBehavior], PolymerElement);

/**
 * @typedef {{
 *  quickStartBluetoothDialog: OobeModalDialog
 * }}
 */
QuickStartEntryPointBase.$;

/** @polymer */
export class QuickStartEntryPoint extends QuickStartEntryPointBase {
  static get is() {
    return 'quick-start-entry-point';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      quickStartTextKey: {
        type: String,
        value: '',
      },
    };
  }

  quickStartButtonClicked_() {
    this.dispatchEvent(new CustomEvent('activate-quick-start', {
      bubbles: true,
      composed: true,
      detail: {enableBluetooth: false},
    }));
  }

  showQuickStartBluetoothDialog() {
    this.$.quickStartBluetoothDialog.showDialog();
  }

  cancelBluetoothDialog_() {
    this.$.quickStartBluetoothDialog.hideDialog();
  }

  turnOnBluetooth_() {
    this.$.quickStartBluetoothDialog.hideDialog();
    this.dispatchEvent(new CustomEvent('activate-quick-start', {
      bubbles: true,
      composed: true,
      detail: {enableBluetooth: true},
    }));
  }
}

customElements.define(QuickStartEntryPoint.is, QuickStartEntryPoint);
