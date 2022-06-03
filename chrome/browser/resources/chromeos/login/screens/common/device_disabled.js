// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Device Disabled message screen.
 */

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {LoginScreenBehaviorInterface}
 */
const DeviceDisabledElementBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    Polymer.Element);

/**
 * @typedef {{
 *   dialog:  OobeAdaptiveDialogElement,
 * }}
 */
DeviceDisabledElementBase.$;

class DeviceDisabled extends DeviceDisabledElementBase {
  static get is() {
    return 'device-disabled-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      /**
       * Device serial number
       * The serial number of the device.
       * @private
       */
      serial_: {
        type: String,
      },

      /**
       * The domain that owns the device (can be empty).
       * @private
       */
      enrollmentDomain_: {
        type: String,
      },

      /**
       * Admin message (external data, non-html-safe).
       * @private
       */
      message_: {
        type: String,
      },
    };
  }

  constructor() {
    super();
    // Properties:
    this.serial_ = '';
    this.enrollmentDomain_ = '';
    this.message_ = '';
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('DeviceDisabledScreen', {
      resetAllowed: false,
    });
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
   * @param {Object} data Screen init payload
   */
  onBeforeShow(data) {
    if ('serial' in data)
      this.serial_ = data.serial;
    if ('domain' in data)
      this.enrollmentDomain_ = data.domain;
    if ('message' in data)
      this.message_ = data.message;
  }

  /**
   * Sets the message to show to the user.
   * @param {string} message The message to show to the user.
   */
  setMessage(message) {
    this.message_ = message;
  }

  /**
   * Updates the explanation shown to the user. The explanation will indicate
   * the device serial number and that it is owned by |domain|. If |domain| is
   * null or empty, a generic explanation will be used instead that does not
   * reference any domain.
   */
  disabledText_(locale, serial, domain) {
    if (domain) {
      return this.i18n('deviceDisabledExplanationWithDomain', serial, domain);
    }
    return this.i18n('deviceDisabledExplanationWithoutDomain', serial);
  }
}

customElements.define(DeviceDisabled.is, DeviceDisabled);
