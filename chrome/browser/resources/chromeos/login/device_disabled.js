// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Offline message screen implementation.
 */

Polymer({
  is: 'device-disabled-element',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  EXTERNAL_API: ['setMessage'],

  properties: {
    /**
     * Device serial number
     * The serial number of the device.
     */
    serial_: {
      type: String,
      value: '',
    },

    /**
     * The domain that owns the device (can be empty).
     */
    enrollmentDomain_: {
      type: String,
      value: '',
    },

    /**
     * Admin message (external data, non-html-safe).
     */
    message_: {
      type: String,
      value: '',
    },
  },


  ready() {
    this.initializeLoginScreen('DeviceDisabledScreen', {
      resetAllowed: false,
    });
  },

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.BLOCKING;
  },

  /**
   * Returns default event target element.
   * @type {Object}
   */
  get defaultControl() {
    return this.$.dialog;
  },

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
  },

  /**
   * Sets the message to show to the user.
   * @param {string} message The message to show to the user.
   */
  setMessage(message) {
    this.message_ = message;
  },

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
  },
});
