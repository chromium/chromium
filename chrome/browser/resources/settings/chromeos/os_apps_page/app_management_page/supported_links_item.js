// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

Polymer({
  is: 'app-management-supported-links-item',

  behaviors: [
    app_management.AppManagementStoreClient,
    I18nBehavior,
  ],

  properties: {
    /** @type {!App} */
    app: Object,

    /**
     * @private {boolean}
     */
    appManagementIntentSettingsEnabled_: {
      type: Boolean,
      value: () =>
          loadTimeData.getBoolean('appManagementIntentSettingsEnabled'),
    },
  },

  attached() {
    this.watch('app', state => app_management.util.getSelectedApp(state));
    this.updateFromStore();
  },

  /**
   * @param {App} app
   * @return {string} Supported or not for radio buttons.
   * @private
   */
  getCurrentPref_(app) {
    return app.isPreferredApp ? 'preferred' : 'browser';
  },

  /**
   * @param {App} app
   * @returns {boolean}
   * @private
   */
  shouldShowIntentSettings_(app) {
    return this.appManagementIntentSettingsEnabled_ &&
        app.supportedLinks.length > 0;
  },

  /**
   * @private
   * @param {App} app
   * @return {string} label for app name radio button
   */
  getAppNameRadioButtonLabel_(app) {
    return this.i18n(
        'appManagementIntentSharingOpenAppLabel', String(app.title));
  },

  /**
   * @param {!App} app
   * @return {boolean}
   * @private
   */
  isInTabMode_(app) {
    return app.type === AppType.kWeb &&
        app.windowMode === apps.mojom.WindowMode.kBrowser;
  },

  /**
   * @param {App} app
   * @return {string} label for app name radio button
   * @private
   */
  getAppNameTabModeExplanation_(app) {
    return this.i18n(
        'appManagementIntentSharingTabExplanation', String(app.title));
  },

  /**
   * @param {!CustomEvent<{value: string}>} event
   * @private
   */
  onSupportedLinkPrefChanged_(event) {
    const newPref = event.detail.value === 'preferred';
    app_management.BrowserProxy.getInstance().handler.setPreferredApp(
        this.app.id, newPref);

    const userAction = newPref ? AppManagementUserAction.PreferredAppTurnedOn :
                                 AppManagementUserAction.PreferredAppTurnedOff;
    app_management.util.recordAppManagementUserAction(
        this.app.type, userAction);
  }
});
