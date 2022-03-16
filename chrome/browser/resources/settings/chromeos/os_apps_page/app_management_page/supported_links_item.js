// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './supported_links_overlapping_apps_dialog.js';
import './supported_links_dialog.js';
import '//resources/cr_components/localized_link/localized_link.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.m.js';

import {AppManagementUserAction, AppType, WindowMode} from '//resources/cr_components/app_management/constants.js';
import {assert} from '//resources/js/assert.m.js';
import {focusWithoutInk} from '//resources/js/cr/ui/focus_without_ink.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {recordAppManagementUserAction} from 'chrome://resources/cr_components/app_management/util.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {recordSettingChange} from '../../metrics_recorder.m.js';

import {BrowserProxy} from './browser_proxy.js';
import {AppManagementStoreClient} from './store_client.js';

const PREFERRED_APP_PREF = 'preferred';

Polymer({
  _template: html`{__html_template__}`,
  is: 'app-management-supported-links-item',

  behaviors: [
    AppManagementStoreClient,
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

    /**
     * @private {boolean}
     */
    showOverlappingAppsDialog_: {
      type: Boolean,
      value: false,
    },

    /**
     * @private {string}
     */
    overlappingAppsWarning_: {
      type: String,
    },

    /**
     * @private {boolean}
     */
    showOverlappingAppsWarning_: {
      type: Boolean,
      value: false,
    },

    /**
     * @type {AppMap}
     * @private
     */
    apps_: {
      type: Object,
    },

    /**
     * @private {Array<string>}
     */
    overlappingAppIds_: {
      type: Array,
    },

  },

  attached() {
    this.watch('apps_', state => state.apps);
    this.updateFromStore();
  },

  observers: [
    'getOverlappingAppsWarning_(apps_, app)',
  ],

  /**
   * The supported links item is not available when an app has no supported
   * links.
   *
   * @param {!App} app
   * @returns {boolean}
   * @private
   */
  isHidden_(app) {
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
    return app.type === AppType.kWeb && app.windowMode === WindowMode.kBrowser;
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
   * @param {!AppMap} apps
   * @param {!App} app
   * @private
   */
  async getOverlappingAppsWarning_(apps, app) {
    if (app === undefined || app.isPreferredApp || apps === undefined) {
      this.showOverlappingAppsWarning_ = false;
      return;
    }

    let overlappingAppIds = [];
    try {
      const {appIds: appIds} =
          await BrowserProxy.getInstance().handler.getOverlappingPreferredApps(
              app.id);
      overlappingAppIds = appIds;
    } catch (err) {
      // If we fail to get the overlapping preferred apps, do not
      // show the overlap warning.
      console.log(err);
      this.showOverlappingAppsWarning_ = false;
      return;
    }
    this.overlappingAppIds_ = overlappingAppIds;

    const appNames = overlappingAppIds.map(app_id => {
      assert(apps[app_id]);
      return apps[app_id].title;
    });

    if (appNames.length === 0) {
      this.showOverlappingAppsWarning_ = false;
      return;
    }

    switch (appNames.length) {
      case 1:
        this.overlappingAppsWarning_ =
            this.i18n('appManagementIntentOverlapWarningText1App', appNames[0]);
        break;
      case 2:
        this.overlappingAppsWarning_ = this.i18n(
            'appManagementIntentOverlapWarningText2Apps', ...appNames);
        break;
      case 3:
        this.overlappingAppsWarning_ = this.i18n(
            'appManagementIntentOverlapWarningText3Apps', ...appNames);
        break;
      case 4:
        this.overlappingAppsWarning_ = this.i18n(
            'appManagementIntentOverlapWarningText4Apps',
            ...appNames.slice(0, 3));
        break;
      default:
        this.overlappingAppsWarning_ = this.i18n(
            'appManagementIntentOverlapWarningText5OrMoreApps',
            ...appNames.slice(0, 3), appNames.length - 3);
        break;
    }

    this.showOverlappingAppsWarning_ = true;
  },

  /* Supported links list dialog functions ************************************/
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

    recordSettingChange();
    recordAppManagementUserAction(
        this.app.type, AppManagementUserAction.SUPPORTED_LINKS_LIST_SHOWN);
  },

  /**
   * @private
   */
  onDialogClose_() {
    this.showSupportedLinksDialog_ = false;
    focusWithoutInk(assert(this.$.heading));
  },

  /* Preferred app state change dialog and related functions ******************/

  /**
   * @param {!CustomEvent<{value: string}>} event
   * @private
   */
  async onSupportedLinkPrefChanged_(event) {
    const preference = event.detail.value;

    let overlappingAppIds = [];

    try {
      const {appIds: appIds} =
          await BrowserProxy.getInstance().handler.getOverlappingPreferredApps(
              this.app.id);
      overlappingAppIds = appIds;
    } catch (err) {
      // If we fail to get the overlapping preferred apps, don't prevent the
      // user from setting their preference.
      console.log(err);
    }

    // If there are overlapping apps, show the overlap dialog to the user.
    if (preference === PREFERRED_APP_PREF && overlappingAppIds.length > 0) {
      this.overlappingAppIds_ = overlappingAppIds;
      this.showOverlappingAppsDialog_ = true;
      recordAppManagementUserAction(
          this.app.type, AppManagementUserAction.OVERLAPPING_APPS_DIALOG_SHOWN);
      return;
    }

    this.setAppAsPreferredApp_(preference);
  },

  onOverlappingDialogClosed_() {
    this.showOverlappingAppsDialog_ = false;

    if (this.shadowRoot.querySelector('#overlap-dialog').wasConfirmed()) {
      this.setAppAsPreferredApp_(PREFERRED_APP_PREF);
      // Return keyboard focus to the preferred radio button.
      focusWithoutInk(this.$.preferred);
    } else {
      // Reset the radio button.
      this.shadowRoot.querySelector('#radio-group').selected =
          this.getCurrentPref_(this.app);
      // Return keyboard focus to the browser radio button.
      focusWithoutInk(this.$.browser);
    }
  },

  /**
   * Sets this.app as a preferred app or not depending on the value of
   * |preference|.
   *
   * @param {string} preference either "preferred" or "browser"
   */
  setAppAsPreferredApp_(preference) {
    const newState = preference === PREFERRED_APP_PREF;

    BrowserProxy.getInstance().handler.setPreferredApp(this.app.id, newState);

    recordSettingChange();
    const userAction = newState ?
        AppManagementUserAction.PREFERRED_APP_TURNED_ON :
        AppManagementUserAction.PREFERRED_APP_TURNED_OFF;
    recordAppManagementUserAction(this.app.type, userAction);
  },
});
