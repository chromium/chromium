// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Item in <os-saved-devices-list> that displays information for a saved
 * Bluetooth device.
 */

import '../../settings_shared.css.js';
import '../os_icons.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';

import {addWebUIListener, removeWebUIListener, WebUIListener} from 'chrome://resources/js/cr.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FastPairSavedDevice} from './settings_fast_pair_constants.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsSavedDevicesListItemElementBase =
    mixinBehaviors([I18nBehavior, WebUIListenerBehavior], PolymerElement);

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

      itemIndex: Number,
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
  onRemoveTap_(event) {
    this.fireRemoveSavedDevice_();
    /** @type {!CrActionMenuElement} */ (this.$.dotsMenu).close();
    event.preventDefault();
    event.stopPropagation();
  }

  /**
   * Fires a 'remove-saved-device' event with device.account_key as the details.
   * @private
   */
  fireRemoveSavedDevice_() {
    const event = new CustomEvent(
        'remove-saved-device',
        {bubbles: true, composed: true, detail: {key: this.device.accountKey}});
    this.dispatchEvent(event);
  }
}

customElements.define(
    SettingsSavedDevicesListItemElement.is,
    SettingsSavedDevicesListItemElement);
