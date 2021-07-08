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

  listeners: {
    click: 'onClick_',
  },

  attached() {
    this.watch('app', state => app_management.util.getSelectedApp(state));
    this.updateFromStore();
  },

  /**
   * @private
   * @param {App} app
   * @return {string} Supported or not for radio buttons.
   */
  getSelectedRadioButtonName_(app) {
    return app.isPreferredApp ? 'preferred' : 'browser';
  },

  /**
   * @private
   * @param {App} app
   * @returns {boolean}
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
   * @private
   * @param {!App} app
   * @return {boolean}
   */
  isInTabMode_(app) {
    return app.type === AppType.kWeb &&
        app.windowMode === apps.mojom.WindowMode.kBrowser;
  },

  /**
   * @private
   * @param {App} app
   * @return {string} label for app name radio button
   */
  getAppNameTabModeExplanation_(app) {
    return this.i18n(
        'appManagementIntentSharingTabExplanation', String(app.title));
  },

  /** @private */
  onClick_() {
    const newState = !this.app.isPreferredApp;
    app_management.BrowserProxy.getInstance().handler.setPreferredApp(
        this.app.id,
        newState,
    );
    const userAction = newState ? AppManagementUserAction.PreferredAppTurnedOn :
                                  AppManagementUserAction.PreferredAppTurnedOff;
    app_management.util.recordAppManagementUserAction(
        this.app.type, userAction);
  }
});
