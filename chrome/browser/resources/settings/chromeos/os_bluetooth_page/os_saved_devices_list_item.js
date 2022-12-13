// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Item in <os-saved-devices-list> that displays information for a saved
 * Bluetooth device.
 */

import '../../settings_shared.css.js';
import '../os_settings_icons.html.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';

import {FastPairSavedDevicesUiEvent, recordSavedDevicesUiEventMetrics} from 'chrome://resources/ash/common/bluetooth/bluetooth_metrics_utils.js';
import {FocusRowBehavior, FocusRowBehaviorInterface} from 'chrome://resources/ash/common/focus_row_behavior.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {addWebUIListener, removeWebUIListener, WebUIListener} from 'chrome://resources/ash/common/cr.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FastPairSavedDevice} from './settings_fast_pair_constants.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 * @implements {FocusRowBehaviorInterface}
 */
const SettingsSavedDevicesListItemElementBase = mixinBehaviors(
    [I18nBehavior, WebUIListenerBehavior, FocusRowBehavior], PolymerElement);

/** @polymer */
class SettingsSavedDevicesListItemElement extends
    SettingsSavedDevicesListItemElementBase {
  static get is() {
    return 'os-settings-saved-devices-list-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @protected {!FastPairSavedDevice}
       */
      device: {
        type: Object,
      },

      /** The index of this item in its parent list, used for its a11y label. */
      itemIndex: Number,

      /**
       * The total number of elements in this item's parent list, used for its
       * a11y label.
       */
      listSize: Number,

      /** @protected */
      shouldShowRemoveSavedDeviceDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  /**
   * @param {!FastPairSavedDevice} device
   * @private
   */
  getDeviceName_(device) {
    return device.name;
  }

  /**
   * @param {!FastPairSavedDevice} device
   * @private
   */
  getImageSrc_(device) {
    return device.imageUrl;
  }

  /**
   * @param {!Event} event
   * @private
   */
  onMenuButtonTap_(event) {
    const button = event.target;
    /** @type {!CrActionMenuElement} */ (this.$.dotsMenu)
        .showAt(/** @type {!HTMLElement} */ (button));
    event.stopPropagation();
  }

  /** @private */
  onRemoveClick_() {
    recordSavedDevicesUiEventMetrics(
        FastPairSavedDevicesUiEvent.SETTINGS_SAVED_DEVICE_LIST_REMOVE_DIALOG);
    this.shouldShowRemoveSavedDeviceDialog_ = true;
    this.$.dotsMenu.close();
  }

  /** @private */
  onCloseRemoveDeviceDialog_() {
    this.shouldShowRemoveSavedDeviceDialog_ = false;
  }

  /**
   * @param {!FastPairSavedDevice} device
   * @return {string}
   * @private
   */
  getAriaLabel_(device) {
    const deviceName = this.getDeviceName_(device);
    return this.i18n(
        'savedDeviceItemA11yLabel', this.itemIndex + 1, this.listSize,
        deviceName);
  }
  /**
   * @param {!FastPairSavedDevice} device
   * @return {string}
   * @private
   */
  getSubpageButtonA11yLabel_(device) {
    const deviceName = this.getDeviceName_(device);
    return this.i18n('savedDeviceItemButtonA11yLabel', deviceName);
  }
}

customElements.define(
    SettingsSavedDevicesListItemElement.is,
    SettingsSavedDevicesListItemElement);
