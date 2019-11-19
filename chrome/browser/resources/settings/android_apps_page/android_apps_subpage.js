// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'android-apps-subpage' is the settings subpage for managing android apps.
 */

Polymer({
  is: 'settings-android-apps-subpage',

  behaviors: [I18nBehavior, PrefsBehavior],

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
      value: function() {
        return this.i18nAdvanced(
            'androidAppsDisableDialogMessage',
            {substitutions: [], tags: ['br']});
      }
    }
  },

  /** @private */
  onPlayStoreEnabledChanged_: function(enabled) {
    if (!enabled &&
        settings.getCurrentRoute() == settings.routes.ANDROID_APPS_DETAILS) {
      settings.navigateToPreviousRoute();
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  computePlayStoreEnabled_: function() {
    return this.androidAppsInfo.playStoreEnabled;
  },

  /**
   * @return {boolean}
   * @private
   */
  allowRemove_: function() {
    return this.prefs.arc.enabled.enforcement !=
        chrome.settingsPrivate.Enforcement.ENFORCED;
  },

  /**
   * Shows a confirmation dialog when disabling android apps.
   * @param {!Event} event
   * @private
   */
  onRemoveTap_: function(event) {
    this.$.confirmDisableDialog.showModal();
  },

  /**
   * Handles the shared proxy confirmation dialog 'Confirm' button.
   * @private
   */
  onConfirmDisableDialogConfirm_: function() {
    this.setPrefValue('arc.enabled', false);
    this.$.confirmDisableDialog.close();
    // Sub-page will be closed in onAndroidAppsInfoUpdate_ call.
  },

  /**
   * Handles the shared proxy confirmation dialog 'Cancel' button or a cancel
   * event.
   * @private
   */
  onConfirmDisableDialogCancel_: function() {
    this.$.confirmDisableDialog.close();
  },

  /** @private */
  onConfirmDisableDialogClose_: function() {
    cr.ui.focusWithoutInk(assert(this.$$('#remove')));
  },

  /**
   * @param {!MouseEvent} event
   * @private
   */
  onManageAndroidAppsTap_: function(event) {
    // |event.detail| is the click count. Keyboard events will have 0 clicks.
    const isKeyboardAction = event.detail == 0;
    settings.AndroidAppsBrowserProxyImpl.getInstance().showAndroidAppsSettings(
        isKeyboardAction);
  },
});
