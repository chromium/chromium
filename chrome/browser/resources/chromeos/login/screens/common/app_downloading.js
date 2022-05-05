// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design App Downloading
 * screen.
 */

/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const AppDownloadingBase = Polymer.mixinBehaviors(
  [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
  Polymer.Element);

class AppDownloading extends AppDownloadingBase {
  static get is() {
    return 'app-downloading-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {};
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('AppDownloadingScreen');
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.ONBOARDING;
  }

  /**
   * Returns the control which should receive initial focus.
   */
  get defaultControl() {
    return this.$['app-downloading-dialog'];
  }

  /** Called when dialog is shown */
  onBeforeShow() {
    if (this.$.downloadingApps) {
      this.$.downloadingApps.playing = true;
    }
  }

  /** Called when dialog is hidden */
  onBeforeHide() {
    if (this.$.downloadingApps) {
      this.$.downloadingApps.playing = false;
    }
  }

  /** @private */
  onContinue_() {
    this.userActed('appDownloadingContinueSetup');
  }
}

customElements.define(AppDownloading.is, AppDownloading);
