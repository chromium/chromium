// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './activity_log_stream.js';
import '../strings.m.js';
import '../shared_style.js';
import '../shared_vars.js';

import {CrContainerShadowBehavior} from 'chrome://resources/cr_elements/cr_container_shadow_behavior.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {afterNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ItemBehavior} from '../item_behavior.js';
import {navigation, Page} from '../navigation_helper.js';

import {ActivityLogDelegate} from './activity_log_history.js';

/**
 * Subpages/views for the activity log. HISTORY shows extension activities
 * fetched from the activity log database with some fields such as args
 * omitted. STREAM displays extension activities in a more verbose format in
 * real time. NONE is used when user is away from the page.
 * @enum {number}
 */
const ActivityLogSubpage = {
  NONE: -1,
  HISTORY: 0,
  STREAM: 1
};

/**
 * A struct used as a placeholder for chrome.developerPrivate.ExtensionInfo
 * for this component if the extensionId from the URL does not correspond to
 * installed extension.
 * @typedef {{
 *   id: string,
 *   isPlaceholder: boolean,
 * }}
 */
export let ActivityLogExtensionPlaceholder;

Polymer({
  is: 'extensions-activity-log',

  _template: html`{__html_template__}`,

  behaviors: [
    CrContainerShadowBehavior,
    I18nBehavior,
    ItemBehavior,
  ],

  properties: {
    /**
     * The underlying ExtensionInfo for the details being displayed.
     * @type {!chrome.developerPrivate.ExtensionInfo|
     *        !ActivityLogExtensionPlaceholder}
     */
    extensionInfo: Object,

    /** @type {!ActivityLogDelegate} */
    delegate: Object,

    /** @private {!ActivityLogSubpage} */
    selectedSubpage_: {
      type: Number,
      value: ActivityLogSubpage.NONE,
      observer: 'onSelectedSubpageChanged_',
    },

    /** @private {Array<string>} */
    tabNames_: {
      type: Array,
      value: () => ([
        loadTimeData.getString('activityLogHistoryTabHeading'),
        loadTimeData.getString('activityLogStreamTabHeading'),
      ]),
    }
  },

  listeners: {
    'view-enter-start': 'onViewEnterStart_',
    'view-exit-finish': 'onViewExitFinish_',
  },

  /**
   * Focuses the back button when page is loaded and set the activie view to
   * be HISTORY when we navigate to the page.
   * @private
   */
  onViewEnterStart_: function() {
    this.selectedSubpage_ = ActivityLogSubpage.HISTORY;
    afterNextRender(this, () => focusWithoutInk(this.$.closeButton));
  },

  /**
   * Set |selectedSubpage_| to NONE to remove the active view from the DOM.
   * @private
   */
  onViewExitFinish_: function() {
    this.selectedSubpage_ = ActivityLogSubpage.NONE;
    // clear the stream if the user is exiting the activity log page.
    const activityLogStream = this.$$('activity-log-stream');
    if (activityLogStream) {
      activityLogStream.clearStream();
    }
  },

  /**
   * @private
   * @return {string}
   */
  getActivityLogHeading_: function() {
    const headingName = this.extensionInfo.isPlaceholder ?
        this.i18n('missingOrUninstalledExtension') :
        this.extensionInfo.name;
    return this.i18n('activityLogPageHeading', headingName);
  },

  /**
   * @private
   * @return {boolean}
   */
  isHistoryTabSelected_: function() {
    return this.selectedSubpage_ === ActivityLogSubpage.HISTORY;
  },

  /**
   * @private
   * @return {boolean}
   */
  isStreamTabSelected_: function() {
    return this.selectedSubpage_ === ActivityLogSubpage.STREAM;
  },

  /**
   * @private
   * @param {!ActivityLogSubpage} newTab
   * @param {!ActivityLogSubpage} oldTab
   */
  onSelectedSubpageChanged_: function(newTab, oldTab) {
    const activityLogStream = this.$$('activity-log-stream');
    if (activityLogStream) {
      if (newTab === ActivityLogSubpage.STREAM) {
        // Start the stream if the user is switching to the real-time tab.
        // This will not handle the first tab switch to the real-time tab as
        // the stream has not been attached to the DOM yet, and is handled
        // instead by the stream's |attached| method.
        activityLogStream.startStream();
      } else if (oldTab === ActivityLogSubpage.STREAM) {
        // Pause the stream if the user is navigating away from the real-time
        // tab.
        activityLogStream.pauseStream();
      }
    }
  },

  /** @private */
  onCloseButtonTap_: function() {
    if (this.extensionInfo.isPlaceholder) {
      navigation.navigateTo({page: Page.LIST});
    } else {
      navigation.navigateTo(
          {page: Page.DETAILS, extensionId: this.extensionInfo.id});
    }
  },
});
