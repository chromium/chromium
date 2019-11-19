// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'android-apps-page' is the settings page for enabling android apps.
 */

Polymer({
  is: 'settings-android-apps-page',

  behaviors: [I18nBehavior, PrefsBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    havePlayStoreApp: Boolean,

    androidAppsInfo: Object,

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        const map = new Map();
        if (settings.routes.ANDROID_APPS_DETAILS) {
          map.set(
              settings.routes.ANDROID_APPS_DETAILS.path,
              '#android-apps .subpage-arrow');
        }
        return map;
      },
    },
  },

  /**
   * @param {!Event} event
   * @private
   */
  onEnableTap_: function(event) {
    this.setPrefValue('arc.enabled', true);
    event.stopPropagation();
  },

  /** @return {boolean} */
  isEnforced_: function(pref) {
    return pref.enforcement == chrome.settingsPrivate.Enforcement.ENFORCED;
  },

  /** @private */
  onSubpageTap_: function(event) {
    if (event.target && event.target.tagName == 'A') {
      // Filter out events coming from 'Learn more' link
      return;
    }
    if (this.androidAppsInfo.playStoreEnabled) {
      settings.navigateTo(settings.routes.ANDROID_APPS_DETAILS);
    }
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
