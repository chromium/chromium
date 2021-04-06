// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-check-page' is the settings page containing the browser
 * safety check.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared_css.js';
import './safety_check_extensions_child.js';
import './safety_check_passwords_child.js';
import './safety_check_safe_browsing_child.js';
import './safety_check_updates_child.js';

// <if expr="_google_chrome and is_win">
import './safety_check_chrome_cleaner_child.js';
// </if>

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {flush, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HatsBrowserProxyImpl} from '../hats_browser_proxy.js';
import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl, SafetyCheckInteractions} from '../metrics_browser_proxy.js';

import {routes} from '../route.js';
import {Router} from '../router.js';
import {SafetyCheckBrowserProxy, SafetyCheckBrowserProxyImpl, SafetyCheckCallbackConstants, SafetyCheckParentStatus} from './safety_check_browser_proxy.js';

/**
 * @typedef {{
 *   newState: SafetyCheckParentStatus,
 *   displayString: string,
 * }}
 */
let ParentChangedEvent;

Polymer({
  is: 'settings-safety-check-page',

  _template: html`{__html_template__}`,

  behaviors: [
    WebUIListenerBehavior,
    I18nBehavior,
  ],

  properties: {
    /**
     * Current state of the safety check parent element.
     * @private {!SafetyCheckParentStatus}
     */
    parentStatus_: {
      type: Number,
      value: SafetyCheckParentStatus.BEFORE,
    },

    /**
     * UI string to display for the parent status.
     * @private
     */
    parentDisplayString_: String,
  },

  /** @private {?SafetyCheckBrowserProxy} */
  safetyCheckBrowserProxy_: null,

  /** @private {?MetricsBrowserProxy} */
  metricsBrowserProxy_: null,

  /**
   * Timer ID for periodic update.
   * @private {number}
   */
  updateTimerId_: -1,

  /** @override */
  attached: function() {
    this.safetyCheckBrowserProxy_ = SafetyCheckBrowserProxyImpl.getInstance();
    this.metricsBrowserProxy_ = MetricsBrowserProxyImpl.getInstance();

    // Register for safety check status updates.
    this.addWebUIListener(
        SafetyCheckCallbackConstants.PARENT_CHANGED,
        this.onSafetyCheckParentChanged_.bind(this));

    // Configure default UI.
    this.parentDisplayString_ =
        this.i18n('safetyCheckParentPrimaryLabelBefore');

    if (Router.getInstance().getCurrentRoute() === routes.SAFETY_CHECK &&
        Router.getInstance().getQueryParameters().has('activateSafetyCheck')) {
      this.runSafetyCheck_();
    }
  },

  /**
   * Triggers the safety check.
   * @private
   */
  runSafetyCheck_: function() {
    // Log click both in action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
        SafetyCheckInteractions.RUN_SAFETY_CHECK);
    this.metricsBrowserProxy_.recordAction('Settings.SafetyCheck.Start');

    // Trigger safety check.
    this.safetyCheckBrowserProxy_.runSafetyCheck();
    // Readout new safety check status via accessibility.
    this.fire('iron-announce', {
      text: this.i18n('safetyCheckAriaLiveRunning'),
    });
  },

  /**
   * @param {!ParentChangedEvent} event
   * @private
   */
  onSafetyCheckParentChanged_: function(event) {
    this.parentStatus_ = event.newState;
    this.parentDisplayString_ = event.displayString;
    if (this.parentStatus_ === SafetyCheckParentStatus.CHECKING) {
      // Ensure the re-run button is visible and focus it.
      flush();
      this.focusIconButton_();
    } else if (this.parentStatus_ === SafetyCheckParentStatus.AFTER) {
      // Start periodic safety check parent ran string updates.
      const update = async () => {
        this.parentDisplayString_ =
            await this.safetyCheckBrowserProxy_.getParentRanDisplayString();
      };
      clearInterval(this.updateTimerId_);
      this.updateTimerId_ = setInterval(update, 60000);
      // Run initial safety check parent ran string update now.
      update();
      // Readout new safety check status via accessibility.
      this.fire('iron-announce', {
        text: this.i18n('safetyCheckAriaLiveAfter'),
      });
    }
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowParentButton_: function() {
    return this.parentStatus_ === SafetyCheckParentStatus.BEFORE;
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowParentIconButton_: function() {
    return this.parentStatus_ !== SafetyCheckParentStatus.BEFORE;
  },

  /** @private */
  onRunSafetyCheckClick_: function() {
    HatsBrowserProxyImpl.getInstance().tryShowSurvey();

    this.runSafetyCheck_();
  },

  /** @private */
  focusIconButton_() {
    const element =
        /** @type {!Element} */ (this.$$('#safetyCheckParentIconButton'));
    element.focus();
  },

  /**
   * @private
   * @return {boolean}
   */
  shouldShowChildren_: function() {
    return this.parentStatus_ !== SafetyCheckParentStatus.BEFORE;
  },
});
