// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview
 * Settings dialog is used to remove a bluetooth saved device from
 * the user's account.
 */
import '../../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {FastPairSavedDevicesUiEvent, recordSavedDevicesUiEventMetrics} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_metrics_utils.js';
import {getDeviceName} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_utils.js';
import {addWebUIListener, removeWebUIListener, WebUIListener} from 'chrome://resources/js/cr.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';

import {FastPairSavedDevice} from './settings_fast_pair_constants.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsBluetoothRemoveSavedDeviceDialogElementBase =
    mixinBehaviors([I18nBehavior, WebUIListenerBehavior], PolymerElement);
/** @polymer */
class SettingsBluetoothRemoveSavedDeviceDialogElement extends
    SettingsBluetoothRemoveSavedDeviceDialogElementBase {
  static get is() {
    return 'os-settings-bluetooth-remove-saved-device-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @protected {!FastPairSavedDevice}
       */
      device_: {
        type: Object,
      },
    };
  }

  /**
   * @private
   */
  getRemoveDeviceDialogBodyText_() {
    return this.i18n(
        'savedDevicesDialogLabel', this.device_.name,
        loadTimeData.getString('primaryUserEmail'));
  }

  /**
   * @param {!Event} event
   * @private
   */
  onRemoveTap_(event) {
    recordSavedDevicesUiEventMetrics(
        FastPairSavedDevicesUiEvent.SETTINGS_SAVED_DEVICE_LIST_REMOVE);
    const fireEvent = new CustomEvent('remove-saved-device', {
      bubbles: true,
      composed: true,
      detail: {key: this.device_.accountKey},
    });
    this.dispatchEvent(fireEvent);
    this.$.dialog.close();
    event.preventDefault();
    event.stopPropagation();
  }

  /** @private */
  onCancelClick_() {
    this.$.dialog.close();
  }
}
customElements.define(
    SettingsBluetoothRemoveSavedDeviceDialogElement.is,
    SettingsBluetoothRemoveSavedDeviceDialogElement);
