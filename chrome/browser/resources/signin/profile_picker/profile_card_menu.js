// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './profile_picker_shared_css.js';
import './icons.js';

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ManageProfilesBrowserProxy, ManageProfilesBrowserProxyImpl, ProfileState} from './manage_profiles_browser_proxy.js';

/**
 * @typedef {{
 *   BrowsingHistory: number,
 *   Passwords: number,
 *   Bookmarks: number,
 *   Autofill: number,
 * }}
 */
export let Statistics;

/**
 * This is the data structure sent back and forth between C++ and JS.
 * @typedef {{
 *   profilePath: string,
 *   statistics: Statistics,
 * }}
 */
export let StatisticsResult;


/**
 * Profile statistics data types.
 * @enum {string}
 */
const ProfileStatistics = {
  BROWSING_HISTORY: 'BrowsingHistory',
  PASSWORDS: 'Passwords',
  BOOKMARKS: 'Bookmarks',
  AUTOFILL: 'Autofill',
};

Polymer({
  is: 'profile-card-menu',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /**  @type {!ProfileState} */
    profileState: {
      type: Object,
    },

    /**
     * Results of profile statistics, keyed by the suffix of the corresponding
     * data type, as reported by the C++ side.
     * @private {!Object<number>}
     */
    statistics_: {
      type: Object,
      // Will be filled as results are reported.
      value() {
        return {};
      }
    },

    /**
     * List of selected data types.
     * @private {!Array<string>}
     */
    profileStatistics_: {
      type: Object,
      value: [
        ProfileStatistics.BROWSING_HISTORY, ProfileStatistics.PASSWORDS,
        ProfileStatistics.BOOKMARKS, ProfileStatistics.AUTOFILL
      ],
    },

    /** @private */
    removeWarningText_: {
      type: String,
      computed: 'computeRemoveWarningText_(profileState)',
    },

    /** @private */
    removeWarningTitle_: {
      type: String,
      computed: 'computeRemoveWarningTitle_(profileState)',
    },
  },

  /** @private {ManageProfilesBrowserProxy} */
  manageProfilesBrowserProxy_: null,

  /** @override */
  ready() {
    this.manageProfilesBrowserProxy_ =
        ManageProfilesBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'profiles-list-changed', () => this.handleProfilesUpdated_());
    this.addWebUIListener(
        'profile-removed', this.handleProfileRemoved_.bind(this));
    this.addWebUIListener(
        'profile-statistics-received',
        this.handleProfileStatsReceived_.bind(this));
  },

  /**
   * @return {string}
   * @private
   */
  computeRemoveWarningText_() {
    return this.i18n(
        this.profileState.isSyncing ? 'removeWarningSignedInProfile' :
                                      'removeWarningLocalProfile');
  },

  /**
   * @return {string}
   * @private
   */
  computeRemoveWarningTitle_() {
    return this.i18n(
        this.profileState.isSyncing ? 'removeWarningSignedInProfileTitle' :
                                      'removeWarningLocalProfileTitle');
  },

  /**
   * @param {!Event} e
   * @private
   */
  onMoreActionsButtonClicked_(e) {
    e.stopPropagation();
    e.preventDefault();
    this.$.actionMenu.showAt(this.$.moreActionsButton);
    chrome.metricsPrivate.recordUserAction(
        'ProfilePicker_ThreeDottedMenuClicked');
  },

  /**
   * @param {!Event} e
   * @private
   */
  onRemoveButtonClicked_(e) {
    e.stopPropagation();
    e.preventDefault();
    this.dataCounters_ = {};
    this.manageProfilesBrowserProxy_.getProfileStatistics(
        this.profileState.profilePath);
    this.$.actionMenu.close();
    this.$.removeConfirmationDialog.showModal();
    chrome.metricsPrivate.recordUserAction('ProfilePicker_RemoveOptionClicked');
  },

  /**
   * @param {!StatisticsResult} result
   * @private
   */
  handleProfileStatsReceived_(result) {
    if (result.profilePath !== this.profileState.profilePath) {
      return;
    }
    this.statistics_ = result.statistics;
  },

  /**
   * @param {ProfileStatistics} dataType
   * @return {string}
   * @private
   */
  getProfileStatisticText_(dataType) {
    switch (dataType) {
      case ProfileStatistics.BROWSING_HISTORY:
        return this.i18n('removeWarningHistory');
      case ProfileStatistics.PASSWORDS:
        return this.i18n('removeWarningPasswords');
      case ProfileStatistics.BOOKMARKS:
        return this.i18n('removeWarningBookmarks');
      case ProfileStatistics.AUTOFILL:
        return this.i18n('removeWarningAutofill');
      default:
        assertNotReached();
    }
  },

  /**
   * @param {string} dataType
   * @return {string}
   * @private
   */
  getProfileStatisticCount_(dataType) {
    const count = this.statistics_[dataType];
    return (count === undefined) ? this.i18n('removeWarningCalculating') :
                                   count.toString();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onRemoveConfirmationClicked_(e) {
    e.stopPropagation();
    e.preventDefault();
    this.manageProfilesBrowserProxy_.removeProfile(
        this.profileState.profilePath);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onRemoveCancelClicked_(e) {
    this.$.removeConfirmationDialog.cancel();
  },

  /**
   * Ensure any menu is closed on profile list updated.
   * @private
   */
  handleProfilesUpdated_() {
    this.$.actionMenu.close();
  },

  /**
   * Closes the remove confirmation dialog when the profile is removed.
   * @param {string} profilePath
   * @private
   */
  handleProfileRemoved_(profilePath) {
    this.handleProfilesUpdated_();
    if (this.profileState.profilePath === profilePath) {
      this.$.removeConfirmationDialog.close();
    }
  },

  /** @private */
  onCustomizeButtonClicked_() {
    this.manageProfilesBrowserProxy_.openManageProfileSettingsSubPage(
        this.profileState.profilePath);
    this.$.actionMenu.close();
  },
});
