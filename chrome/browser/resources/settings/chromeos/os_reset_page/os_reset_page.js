// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-reset-page' is the OS settings page containing reset
 * settings.
 */
Polymer({
  is: 'os-settings-reset-page',

  behaviors: [DeepLinkingBehavior, settings.RouteObserverBehavior],

  properties: {
    /** @private */
    showPowerwashDialog_: Boolean,

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([chromeos.settings.mojom.Setting.kPowerwash]),
    },
  },

  /** @private */
  /**
   * @param {!Event} e
   * @private
   */
  onShowPowerwashDialog_(e) {
    e.preventDefault();
    this.showPowerwashDialog_ = true;
  },

  /** @private */
  onPowerwashDialogClose_() {
    this.showPowerwashDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$.powerwash));
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} newRoute
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged(newRoute, oldRoute) {
    // Does not apply to this page.
    if (newRoute !== settings.routes.OS_RESET) {
      return;
    }

    this.attemptDeepLink();
  },
});
