// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Oobe} from '../../cr_ui.js';
import {OobeUiState} from '../display_manager_types.js';
import {OobeTypes} from '../oobe_types.js';

/**
 * @fileoverview
 * 'LoginScreenBehavior' is login.Screen API implementation for Polymer objects.
 */

const CALLBACK_USER_ACTED = 'userActed';

/** @polymerBehavior */
export const LoginScreenBehavior = {
  // List of methods exported to login.screenName.<method> API.
  // This is expected to be overridden by the Polymer object using this
  // behavior.
  // @type{!Array<string>}
  EXTERNAL_API: [],

  /**
   * Initialize screen behavior.
   * @param {string} screenName Name of created class (external api prefix).
   */
  initializeLoginScreen(screenName) {
    const api = {};

    if (this.EXTERNAL_API.length !== 0) {
      for (let i = 0; i < this.EXTERNAL_API.length; ++i) {
        const methodName = this.EXTERNAL_API[i];
        if (typeof this[methodName] !== 'function') {
          throw Error(
              'External method "' + methodName + '" for screen "' + screenName +
              '" is not a function or is undefined.');
        }
        api[methodName] = (...args) => this[methodName](...args);
      }
    }
    this.sendPrefix_ = 'login.' + screenName + '.';
    this.registerScreenApi_(screenName, api);
    Oobe.getInstance().registerScreen(this);
  },


  sendPrefix_: undefined,

  userActed(args) {
    if (this.sendPrefix_ === undefined) {
      console.error('LoginScreenBehavior: send prefix is not defined');
      return;
    }
    if (typeof args === 'string') {
      args = [args];
    }
    chrome.send(this.sendPrefix_ + CALLBACK_USER_ACTED, args);
  },

  /* ******************  Default screen API below.  ********************** */


  // If defined, invoked when CANCEL acccelerator is pressed.
  // @type{function()}
  cancel: undefined,

  /**
   * Returns element that will receive focus.
   * @return {Object}
   */
  get defaultControl() {
    return this;
  },

  /**
   * Returns UI state to be used when showing this screen. Default
   * implementation returns OobeUiState.HIDDEN.
   * @return {OobeUiState} The state of the OOBE UI.
   */
  getOobeUIInitialState() {
    return OobeUiState.HIDDEN;
  },

  /**
   * If defined, invoked when tablet mode is changed.
   * Boolean parameter is true when device is in tablet mode.
   * @type {function(boolean)|undefined}
   */
  setTabletModeState: undefined,

  /**
   * If defined, invoked for the currently active screen when screen localized
   * data needs to be updated.
   * @type {function()|undefined}
   */
  updateLocalizedContent: undefined,

  /**
   * If defined, invoked when OOBE configuration is loaded.
   * @type {OobeTypes.OobeConfiguration|undefined} configuration
   */
  updateOobeConfiguration: undefined,

  /**
   * Register external screen API with login object.
   * Example:
   *    this.registerScreenApi_('ScreenName', {
   *         foo() { console.log('foo'); },
   *     });
   *     login.ScreenName.foo(); // valid
   *
   * @param {string} name Name of created class.
   * @param {Object} api Screen API.
   * @private
   */
  registerScreenApi_(name, api) {
    // TODO(crbug.com/1229130) - Improve this.
    if (globalThis.login === undefined) {
      globalThis.login = {};
    }
    globalThis.login[name] = api;
  },
};

/** @interface */
export class LoginScreenBehaviorInterface {
  /** @param {string} screenName */
  initializeLoginScreen(screenName) {}
  /** @param {string|Array<?>} action_id */
  userActed(action_id) {}
  /** @return {OobeUiState} */
  getOobeUIInitialState() {}
  /** @return {!Array<string>} */
  get EXTERNAL_API() {}
  /** @return {HTMLElement|null} */
  get defaultControl() {}
  /** @param {boolean} isInTabletMode */
  setTabletModeState(isInTabletMode) {}
  updateLocalizedContent() {}
  /** @param {!OobeTypes.OobeConfiguration} configuration */
  updateOobeConfiguration(configuration) {}
}
