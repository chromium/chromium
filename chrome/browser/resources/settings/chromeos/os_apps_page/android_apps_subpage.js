// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'android-apps-subpage' is the settings subpage for managing android apps.
 */

Polymer({
  is: 'settings-android-apps-subpage',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    PrefsBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: Object,

    /** @private {!AndroidAppsInfo|undefined} */
    androidAppsInfo: {
      type: Object,
    },

    /** @private */
    playStoreEnabled_: {
      type: Boolean,
      computed: 'computePlayStoreEnabled_(androidAppsInfo)',
      observer: 'onPlayStoreEnabledChanged_'
    },

    /** @private */
    dialogBody_: {
      type: String,
      value() {
        return this.i18nAdvanced(
            'androidAppsDisableDialogMessage',
            {substitutions: [], tags: ['br']});
      }
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kManageAndroidPreferences,
        chromeos.settings.mojom.Setting.kRemovePlayStore,
      ]),
    },
  },

  /**
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.ANDROID_APPS_DETAILS) {
      return;
    }

    this.attemptDeepLink();
  },

  /** @private */
  onPlayStoreEnabledChanged_(enabled) {
    if (!enabled &&
        settings.Router.getInstance().getCurrentRoute() ==
            settings.routes.ANDROID_APPS_DETAILS) {
      settings.Router.getInstance().navigateToPreviousRoute();
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  computePlayStoreEnabled_() {
    return this.androidAppsInfo.playStoreEnabled;
  },

  /**
   * @return {boolean}
   * @private
   */
  allowRemove_() {
    return this.prefs.arc.enabled.enforcement !=
        chrome.settingsPrivate.Enforcement.ENFORCED;
  },

  /**
   * Shows a confirmation dialog when disabling android apps.
   * @param {!Event} event
   * @private
   */
  onRemoveTap_(event) {
    this.$.confirmDisableDialog.showModal();
  },

  /**
   * Handles the shared proxy confirmation dialog 'Confirm' button.
   * @private
   */
  onConfirmDisableDialogConfirm_() {
    this.setPrefValue('arc.enabled', false);
    this.$.confirmDisableDialog.close();
    // Sub-page will be closed in onAndroidAppsInfoUpdate_ call.
  },

  /**
   * Handles the shared proxy confirmation dialog 'Cancel' button or a cancel
   * event.
   * @private
   */
  onConfirmDisableDialogCancel_() {
    this.$.confirmDisableDialog.close();
  },

  /** @private */
  onConfirmDisableDialogClose_() {
    cr.ui.focusWithoutInk(assert(this.$$('#remove')));
  },

  /**
   * @param {!MouseEvent} event
   * @private
   */
  onManageAndroidAppsTap_(event) {
    // |event.detail| is the click count. Keyboard events will have 0 clicks.
    const isKeyboardAction = event.detail == 0;
    settings.AndroidAppsBrowserProxyImpl.getInstance().showAndroidAppsSettings(
        isKeyboardAction);
  },
});
