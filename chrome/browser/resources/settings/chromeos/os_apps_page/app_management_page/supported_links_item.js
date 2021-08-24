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
     * @type {boolean}
     */
    hidden: {
      type: Boolean,
      computed: 'isHidden_(app)',
      reflectToAttribute: true,
    },

    /**
     * @type {boolean}
     * @private
     */
    disabled_: {
      type: Boolean,
      computed: 'isDisabled_(app)',
    },

    /**
     * @private {boolean}
     */
    showSupportedLinksDialog_: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * The supported links item is not available when an app has no supported
   * links.
   *
   * @param {!App} app
   * @returns {boolean}
   * @private
   */
  isHidden_(app) {
    if (!loadTimeData.getBoolean('appManagementIntentSettingsEnabled')) {
      return true;
    }
    return !app.supportedLinks.length;
  },

  /**
   * Disable the radio button options if the app is a PWA and is set to open
   * in the browser.
   *
   * @param {!App} app
   * @returns {boolean} If the preference settings should be disabled
   * @private
   */
  isDisabled_(app) {
    return app.type === AppType.kWeb &&
        app.windowMode === apps.mojom.WindowMode.kBrowser;
  },

  /**
   * @param {!App} app
   * @return {!string} which indicates if the app is currently preferred or not.
   * @private
   */
  getCurrentPref_(app) {
    return app.isPreferredApp ? 'preferred' : 'browser';
  },

  /**
   * @param {!App} app
   * @return {!string} label for 'preferred' radio button
   * @private
   */
  getPreferredLabel_(app) {
    return this.i18n(
        'appManagementIntentSharingOpenAppLabel', String(app.title));
  },

  /**
   * @param {!App} app
   * @return {!string} which explains why the setting is disabled.
   * @private
   */
  getDisabledExplanation_(app) {
    return this.i18nAdvanced(
        'appManagementIntentSharingTabExplanation',
        {substitutions: [String(app.title)]});
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
  },

  /**
   * Stamps and opens the Supported Links dialog.
   * @param {!Event} e
   * @private
   */
  launchDialog_(e) {
    // A place holder href with the value "#" is used to have a compliant link.
    // This prevents the browser from navigating the window to "#"
    e.detail.event.preventDefault();
    e.stopPropagation();
    this.showSupportedLinksDialog_ = true;
  },

  /**
   * @private
   */
  onDialogClose_() {
    this.showSupportedLinksDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$$('#heading')));
  }
});
