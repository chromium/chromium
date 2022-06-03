// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Authenticator} from '../../gaia_auth_host/authenticator.m.js';

/**
 * @typedef {{
 *    primaryActionButtonLabel: string,
 *    primaryActionButtonEnabled: boolean,
 *    secondaryActionButtonLabel: string,
 *    secondaryActionButtonEnabled: boolean,
 * }}
 */
let ActionButtonsData;

/**
 * Event listeners for the events triggered by the authenticator.
 * @type {!Array<!{event: string, field: string}>}
 */
const authenticatorEventListeners = [
  {event: 'setPrimaryActionLabel', field: 'primaryActionButtonLabel'},
  {event: 'setPrimaryActionEnabled', field: 'primaryActionButtonEnabled'},
  {event: 'setSecondaryActionLabel', field: 'secondaryActionButtonLabel'},
  {event: 'setSecondaryActionEnabled', field: 'secondaryActionButtonEnabled'},
];

Polymer({
  is: 'gaia-action-buttons',

  _template: html`{__html_template__}`,

  properties: {
    /**
     * The auth extension host instance.
     * @type {?Authenticator}
     */
    authenticator: {
      type: Object,
      observer: 'authenticatorChanged_',
    },

    roundedButton: {
      type: Boolean,
      value: false,
    },

    actionButtonClasses_: {
      type: String,
      computed: 'getActionButtonClasses_(roundedButton)',
    },

    secondaryButtonClasses_: {
      type: String,
      computed: 'getSecondaryButtonClasses_(roundedButton)',
    },

    /**
     * Controls label and availability on the action buttons.
     * @private {!ActionButtonsData}
     */
    data_: {
      type: Object,
      value() {
        return {
          primaryActionButtonLabel: '',
          primaryActionButtonEnabled: true,
          secondaryActionButtonLabel: '',
          secondaryActionButtonEnabled: true,
        };
      }
    },
  },

  /** @private */
  authenticatorChanged_() {
    if (this.authenticator) {
      this.addAuthExtHostListeners_();
    }
  },

  /** @private */
  addAuthExtHostListeners_() {
    authenticatorEventListeners.forEach(listenParams => {
      this.authenticator.addEventListener(listenParams.event, e => {
        this.set(`data_.${listenParams.field}`, e.detail);
      });
    });
    this.authenticator.addEventListener(
        'setAllActionsEnabled',
        e => this.onSetAllActionsEnabled_(
            /** @type {!CustomEvent<boolean>} */ (e)));
  },

  /**
   * Invoked when the auth host emits 'setAllActionsEnabled' event
   * @param {!CustomEvent<boolean>} e
   * @private
   */
  onSetAllActionsEnabled_(e) {
    this.set('data_.primaryActionButtonEnabled', e.detail);
    this.set('data_.secondaryActionButtonEnabled', e.detail);
  },

  /**
   * Handles clicks on "PrimaryAction" button.
   * @private
   */
  onPrimaryActionButtonClicked_() {
    this.authenticator.sendMessageToWebview('primaryActionHit');
    this.focusWebview_();
  },

  /**
   * Handles clicks on "SecondaryAction" button.
   * @private
   */
  onSecondaryActionButtonClicked_() {
    this.authenticator.sendMessageToWebview('secondaryActionHit');
    this.focusWebview_();
  },

  /** @private */
  focusWebview_() {
    this.fire('set-focus-to-webview');
  },

  /**
   * @private
   * @param {boolean} roundedButton
   * @return {string}
   */
  getActionButtonClasses_(roundedButton) {
    let cssClasses = ['action-button'];
    if (roundedButton) {
      cssClasses.push('rounded-button');
    }
    return cssClasses.join(' ');
  },

  /**
   * @private
   * @param {boolean} roundedButton
   * @return {string}
   */
  getSecondaryButtonClasses_(roundedButton) {
    let cssClasses = ['secondary-button'];
    if (roundedButton) {
      cssClasses.push('rounded-button');
    }
    return cssClasses.join(' ');
  },

  /** @param {Object} authExtHost */
  setAuthExtHostForTest(authExtHost) {
    this.authenticator = /** @type {Authenticator} */ (authExtHost);
    this.addAuthExtHostListeners_();
  },
});
