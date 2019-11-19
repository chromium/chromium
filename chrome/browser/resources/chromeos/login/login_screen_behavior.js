// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'LoginScreenBehavior' is login.Screen API implementation for Polymer objects.
 */

/** @polymerBehavior */
var LoginScreenBehavior = {
  // List of methods exported to login.screenName.<method> API.
  // This is expected to be overridden by the Polymer object using this
  // behavior.
  // @type{!Array<string>}
  EXTERNAL_API: [],

  /**
   * Initialize screen behavior.
   * @param {string} screenName Name of created class (external api prefix).
   * @param {DisplayManagerScreenAttributes} attributes
   */
  initializeLoginScreen: function(screenName, attributes) {
    let api = {};

    if (this.EXTERNAL_API.length != 0) {
      for (var i = 0; i < this.EXTERNAL_API.length; ++i) {
        var methodName = this.EXTERNAL_API[i];
        if (typeof this[methodName] !== 'function') {
          throw Error(
              'External method "' + methodName + '" for screen "' + screenName +
              '" is not a function or is undefined.');
        }
        api[methodName] = this[methodName].bind(this);
      }
    }
    this.registerScreenApi_(screenName, api);
    Oobe.getInstance().registerScreen(this, attributes);
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

  // If defined, invoked when logn.preloadScreen() is called.
  // @type{function()}
  deferredInitialization: undefined,

  /**
   * Returns minimal size that screen prefers to have. Default implementation
   * returns current screen size.
   * @return {{width: number, height: number}}
   */
  getPreferredSize: function() {
    return {width: this.offsetWidth, height: this.offsetHeight};
  },

  /**
   * Screen will ignore accelerators when true.
   * @type {boolean}
   */
  ignoreAccelerators: false,

  /**
   * If defined, invoked for the currently active screen when screen size
   * changes.
   * @type {function()|undefined}
   */
  onWindowResize: undefined,

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
   *         foo: function() { console.log('foo'); },
   *     });
   *     login.ScreenName.foo(); // valid
   *
   * @param {string} name Name of created class.
   * @param {Object} api Screen API.
   * @private
   */
  registerScreenApi_: function(name, api) {
    // Closure compiler incorrectly parses this, so we use cr.define.call(...).
    cr.define.call(cr.define, 'login', function() {
      var result = {};
      result[name] = api;
      return result;
    });
  },
};

/**
 * TODO(alemate): Replace with an interface. b/24294625
 * @typedef {{
 *   attached: function()
 * }}
 */
LoginScreenBehavior.Proto;
