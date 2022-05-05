// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying ARC ADB sideloading screen.
 */

/* #js_imports_placeholder */

/**
 * UI mode for the dialog.
 * @enum {string}
 */
const AdbSideloadingState = {
  ERROR: 'error',
  SETUP: 'setup',
};

/**
 * The constants need to be synced with EnableAdbSideloadingScreenView::UIState
 * @enum {number}
 */
const ADB_SIDELOADING_SCREEN_STATE = {
  ERROR: 1,
  SETUP: 2,
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
 const AdbSideloadingBase = Polymer.mixinBehaviors([OobeI18nBehavior,
  LoginScreenBehavior, MultiStepBehavior], Polymer.Element);

/**
 * @polymer
 */
class AdbSideloading extends AdbSideloadingBase {
  static get is() {
    return 'adb-sideloading-element';
  }

  /* #html_template_placeholder */

  constructor() {
    super();
  }

  get EXTERNAL_API() {
    return ['setScreenState'];
  }

  get UI_STEPS() {
    return AdbSideloadingState;
  }

  defaultUIStep() {
    return AdbSideloadingState.SETUP;
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('EnableAdbSideloadingScreen');
  }

  /*
   * Executed on language change.
   */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
  }

  onBeforeShow(data) {
    this.setScreenState(ADB_SIDELOADING_SCREEN_STATE.SETUP);
  }

  /**
   * Sets UI state for the dialog to show corresponding content.
   * @param {ADB_SIDELOADING_SCREEN_STATE} state
   */
  setScreenState(state) {
    if (state == ADB_SIDELOADING_SCREEN_STATE.ERROR) {
      this.setUIStep(AdbSideloadingState.ERROR);
    } else if (state == ADB_SIDELOADING_SCREEN_STATE.SETUP) {
      this.setUIStep(AdbSideloadingState.SETUP);
    }
  }

  /**
   * On-tap event handler for enable button.
   *
   * @private
   */
  onEnableTap_() {
    this.userActed('enable-pressed');
  }

  /**
   * On-tap event handler for cancel button.
   *
   * @private
   */
  onCancelTap_() {
    this.userActed('cancel-pressed');
  }

  /**
   * On-tap event handler for learn more link.
   *
   * @private
   */
  onLearnMoreTap_() {
    this.userActed('learn-more-link');
  }
}

customElements.define(AdbSideloading.is, AdbSideloading);
