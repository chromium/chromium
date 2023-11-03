// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Device Disabled message screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';
import {OOBE_UI_STATE} from '../../components/display_manager_types.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {LoginScreenBehaviorInterface}
 */
const DeviceDisabledElementBase = mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    PolymerElement);

/**
 * @typedef {{
 *   dialog:  OobeAdaptiveDialog,
 * }}
 */
DeviceDisabledElementBase.$;

/**
 * Data that is passed to the screen during onBeforeShow.
 * @typedef {{
 *   serial: string,
 *   domain: string,
 *   message: string,
 *   isDisabledAdDevice: boolean,
 * }}
 */
let DeviceDisabledScreenData;

/** @polymer */
class DeviceDisabled extends DeviceDisabledElementBase {
  static get is() {
    return 'device-disabled-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The serial number of the device.
       * @type {string}
       * @private
       */
      serial_: {
        type: String,
        value: '',
      },

      /**
       * The domain that owns the device (can be empty).
       * @type {string}
       * @private
       */
      enrollmentDomain_: {
        type: String,
        value: '',
      },

      /**
       * Admin message (external data, non-html-safe).
       * @type {string}
       * @private
       */
      message_: {
        type: String,
        value: '',
      },

      /**
       * Flag indicating if the device was disabled because it's in AD mode,
       * which is no longer supported.
       * @type {boolean}
       * @private
       */
      isDisabledAdDevice_: {
        type: Boolean,
        value: false,
      },
    };
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('DeviceDisabledScreen');
  }

  /** @override */
  get EXTERNAL_API() {
    return ['setMessage'];
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.BLOCKING;
  }

  /**
   * @override
   */
  get defaultControl() {
    return this.$.dialog;
  }

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param {DeviceDisabledScreenData} data Screen init payload.
   */
  onBeforeShow(data) {
    if ('serial' in data) {
      this.serial_ = data.serial;
    }
    if ('domain' in data) {
      this.enrollmentDomain_ = data.domain;
    }
    if ('message' in data) {
      this.message_ = data.message;
    }
    if ('isDisabledAdDevice' in data) {
      this.isDisabledAdDevice_ = data.isDisabledAdDevice;
    }
  }

  /**
   * Sets the message to be shown to the user.
   * @param {string} message The message to be shown to the user.
   */
  setMessage(message) {
    this.message_ = message;
  }

  /**
   * Updates the explanation shown to the user. The explanation contains the
   * device serial number and may contain the domain the device is enrolled to,
   * if that information is available. However, if `isDisabledAdDevice` is true,
   * a custom explanation about Chromad disabling will be used.
   * @param {string} locale The i18n locale.
   * @param {string} serial The device serial number.
   * @param {string} domain The enrollment domain.
   * @param {boolean} isDisabledAdDevice Flag indicating if the device was
   *     disabled because it's in AD mode.
   * @return {string} The internationalized explanation.
   */
  disabledText_(locale, serial, domain, isDisabledAdDevice) {
    if (isDisabledAdDevice) {
      return this.i18nAdvancedDynamic(
          locale, 'deviceDisabledAdModeExplanation', {substitutions: [serial]});
    }
    if (domain) {
      return this.i18nAdvancedDynamic(
          locale, 'deviceDisabledExplanationWithDomain',
          {substitutions: [serial, domain]});
    }
    return this.i18nAdvancedDynamic(
        locale, 'deviceDisabledExplanationWithoutDomain',
        {substitutions: [serial]});
  }
}

customElements.define(DeviceDisabled.is, DeviceDisabled);
