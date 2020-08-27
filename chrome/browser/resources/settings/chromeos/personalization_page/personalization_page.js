// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * 'settings-personalization-page' is the settings page containing
 * personalization settings.
 */
Polymer({
  is: 'settings-personalization-page',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: Object,

    /** @private */
    showWallpaperRow_: {type: Boolean, value: true},

    /** @private */
    isWallpaperPolicyControlled_: {type: Boolean, value: true},

    /** @private */
    isAmbientModeEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isAmbientModeEnabled');
      },
      readOnly: true,
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        if (settings.routes.CHANGE_PICTURE) {
          map.set(settings.routes.CHANGE_PICTURE.path, '#changePictureRow');
        } else if (settings.routes.AMBIENT_MODE) {
          map.set(settings.routes.AMBIENT_MODE.path, '#ambientModeRow');
        }

        return map;
      }
    },
  },

  /** @private {?settings.WallpaperBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.WallpaperBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.browserProxy_.isWallpaperSettingVisible().then(
        isWallpaperSettingVisible => {
          this.showWallpaperRow_ = isWallpaperSettingVisible;
        });
    this.browserProxy_.isWallpaperPolicyControlled().then(
        isPolicyControlled => {
          this.isWallpaperPolicyControlled_ = isPolicyControlled;
        });
  },

  /**
   * @private
   */
  openWallpaperManager_() {
    this.browserProxy_.openWallpaperManager();
  },

  /** @private */
  navigateToChangePicture_() {
    settings.Router.getInstance().navigateTo(settings.routes.CHANGE_PICTURE);
  },

  /** @private */
  navigateToAmbientMode_() {
    settings.Router.getInstance().navigateTo(settings.routes.AMBIENT_MODE);
  },

  /**
   * @param {boolean} toggleValue
   * @return {string}
   * @private
   */
  getAmbientModeRowSubLabel_(toggleValue) {
    return this.i18n(
        toggleValue ? 'ambientModeEnabled' : 'ambientModeDisabled');
  },
});
