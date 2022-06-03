// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe reset screen implementation.
 */

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const AutolaunchBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, OobeDialogHostBehavior],
    Polymer.Element);

/**
 * @polymer
 */
class Autolaunch extends AutolaunchBase {
  static get is() {
    return 'autolaunch-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      appName_: {type: String},
      appIconUrl_: {type: String},
    };
  }

  constructor() {
    super();
    this.appName_ = '';
    this.appIconUrl_ = '';
  }

  /** Overridden from LoginScreenBehavior. */
  // clang-format off
  get EXTERNAL_API() {
    return [
      'updateApp',
    ];
  }
  // clang-format on

  ready() {
    super.ready();
    this.initializeLoginScreen('AutolaunchScreen', {
      resetAllowed: true,
    });
  }

  onConfirm_() {
    chrome.send('autolaunchOnConfirm');
  }

  onCancel_() {
    chrome.send('autolaunchOnCancel');
  }

  /**
   * Event handler invoked when the page is shown and ready.
   */
  onBeforeShow() {
    chrome.send('autolaunchVisible');
  }

  /**
   * Cancels the reset and drops the user back to the login screen.
   */
  cancel() {
    chrome.send('autolaunchOnCancel');
  }

  /**
   * Sets app to be displayed in the auto-launch warning.
   * @param {!Object} app An dictionary with app info.
   */
  updateApp(app) {
    this.appName_ = app.appName;
    if (app.appIconUrl && app.appIconUrl.length)
      this.appIconUrl_ = app.appIconUrl;
  }
}

customElements.define(Autolaunch.is, Autolaunch);
