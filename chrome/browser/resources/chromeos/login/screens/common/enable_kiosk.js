// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Polymer element for displaying material design Enable Kiosk
 * screen.
 */

/* #js_imports_placeholder */

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const EnableKioskMode = {
  CONFIRM: 'confirm',
  SUCCESS: 'success',
  ERROR: 'error',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
 const EnableKioskBase = Polymer.mixinBehaviors(
  [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
  Polymer.Element);

/**
 * @polymer
 */
class EnableKiosk extends EnableKioskBase {

  static get is() { return 'enable-kiosk-element'; }

  /* #html_template_placeholder */

  static get properties() {
    return {
      /**
       * Current dialog state
       * @private
       */
      state_: {
        type: String,
        value: EnableKioskMode.CONFIRM,
      },
    };
  }

  constructor() {
    super();
  }

  get EXTERNAL_API() {
    return  ['onCompleted'];
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('KioskEnableScreen', {
      resetAllowed: true,
    });
  }

  /** Called after resources are updated. */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  }

  /** Called when dialog is shown */
  onBeforeShow() {
    this.state_ = EnableKioskMode.CONFIRM;
  }

  /**
   * "Enable" button handler
   * @private
   */
  onEnableButton_(event) {
    this.userActed('enable');
  }

  /**
   * "Cancel" / "Ok" button handler
   * @private
   */
  closeDialog_(event) {
    this.userActed('close');
  }

  onCompleted(success) {
    this.state_ = success ? EnableKioskMode.SUCCESS : EnableKioskMode.ERROR;
  }

  /**
   * Simple equality comparison function.
   * @private
   */
  eq_(one, another) {
    return one === another;
  }

  /**
   *
   * @private
   */
  primaryButtonTextKey_(state) {
    if (state === EnableKioskMode.CONFIRM)
      return 'kioskOKButton';
    return 'kioskCancelButton';
  }
}

customElements.define(EnableKiosk.is, EnableKiosk);