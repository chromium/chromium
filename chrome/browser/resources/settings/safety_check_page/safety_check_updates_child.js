// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-updates-child' is the settings page containing the safety
 * check child showing the browser's update status.
 */
import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LifetimeBrowserProxy, LifetimeBrowserProxyImpl} from '../lifetime_browser_proxy.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl, SafetyCheckInteractions} from '../metrics_browser_proxy.js';

import {SafetyCheckCallbackConstants, SafetyCheckUpdatesStatus} from './safety_check_browser_proxy.js';
import {SafetyCheckIconStatus} from './safety_check_child.js';

/**
 * @typedef {{
 *   newState: SafetyCheckUpdatesStatus,
 *   displayString: string,
 * }}
 */
let UpdatesChangedEvent;

Polymer({
  is: 'settings-safety-check-updates-child',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * Current state of the safety check updates child.
     * @private {!SafetyCheckUpdatesStatus}
     */
    status_: {
      type: Number,
      value: SafetyCheckUpdatesStatus.CHECKING,
    },

    /**
     * UI string to display for this child, received from the backend.
     * @private
     */
    displayString_: String,
  },

  /** @private {?LifetimeBrowserProxy} */
  lifetimeBrowserProxy_: null,

  /** @private {?MetricsBrowserProxy} */
  metricsBrowserProxy_: null,

  /** @override */
  attached: function() {
    this.lifetimeBrowserProxy_ = LifetimeBrowserProxyImpl.getInstance();
    this.metricsBrowserProxy_ = MetricsBrowserProxyImpl.getInstance();

    // Register for safety check status updates.
    this.addWebUIListener(
        SafetyCheckCallbackConstants.UPDATES_CHANGED,
        this.onSafetyCheckUpdatesChanged_.bind(this));
  },

  /**
   * @param {!UpdatesChangedEvent} event
   * @private
   */
  onSafetyCheckUpdatesChanged_: function(event) {
    this.status_ = event.newState;
    this.displayString_ = event.displayString;
  },

  /**
   * @return {SafetyCheckIconStatus}
   * @private
   */
  getIconStatus_: function() {
    switch (this.status_) {
      case SafetyCheckUpdatesStatus.CHECKING:
      case SafetyCheckUpdatesStatus.UPDATING:
        return SafetyCheckIconStatus.RUNNING;
      case SafetyCheckUpdatesStatus.UPDATED:
        return SafetyCheckIconStatus.SAFE;
      case SafetyCheckUpdatesStatus.RELAUNCH:
      case SafetyCheckUpdatesStatus.DISABLED_BY_ADMIN:
      case SafetyCheckUpdatesStatus.FAILED_OFFLINE:
      case SafetyCheckUpdatesStatus.UNKNOWN:
        return SafetyCheckIconStatus.INFO;
      case SafetyCheckUpdatesStatus.FAILED:
        return SafetyCheckIconStatus.WARNING;
      default:
        assertNotReached();
    }
  },

  /**
   * @private
   * @return {?string}
   */
  getButtonLabel_: function() {
    switch (this.status_) {
      case SafetyCheckUpdatesStatus.RELAUNCH:
        return this.i18n('aboutRelaunch');
      default:
        return null;
    }
  },

  /** @private */
  onButtonClick_: function() {
    // Log click both in action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
        SafetyCheckInteractions.UPDATES_RELAUNCH);
    this.metricsBrowserProxy_.recordAction(
        'Settings.SafetyCheck.RelaunchAfterUpdates');

    this.lifetimeBrowserProxy_.relaunch();
  },

  /**
   * @private
   * @return {?string}
   */
  getManagedIcon_: function() {
    switch (this.status_) {
      case SafetyCheckUpdatesStatus.DISABLED_BY_ADMIN:
        return 'cr20:domain';
      default:
        return null;
    }
  },
});
