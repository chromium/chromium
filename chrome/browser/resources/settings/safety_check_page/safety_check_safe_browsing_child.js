// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-safe-browsing-child' is the settings page containing the
 * safety check child showing the Safe Browsing status.
 */
import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MetricsBrowserProxy, MetricsBrowserProxyImpl, SafetyCheckInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.m.js';

import {SafetyCheckCallbackConstants, SafetyCheckSafeBrowsingStatus} from './safety_check_browser_proxy.js';
import {SafetyCheckIconStatus} from './safety_check_child.js';

/**
 * @typedef {{
 *   newState: SafetyCheckSafeBrowsingStatus,
 *   displayString: string,
 * }}
 */
let SafeBrowsingChangedEvent;

Polymer({
  is: 'settings-safety-check-safe-browsing-child',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * Current state of the safety check safe browsing child.
     * @private {!SafetyCheckSafeBrowsingStatus}
     */
    status_: {
      type: Number,
      value: SafetyCheckSafeBrowsingStatus.CHECKING,
    },

    /**
     * UI string to display for this child, received from the backend.
     * @private
     */
    displayString_: String,

    /**
     * A set of statuses that the entire row is clickable.
     * @type {!Set<!SafetyCheckSafeBrowsingStatus>}
     * @private
     */
    rowClickableStatuses: {
      readOnly: true,
      type: Object,
      value: () => new Set([
        SafetyCheckSafeBrowsingStatus.ENABLED_STANDARD,
        SafetyCheckSafeBrowsingStatus.ENABLED_ENHANCED,
        SafetyCheckSafeBrowsingStatus.ENABLED_STANDARD_AVAILABLE_ENHANCED,
        SafetyCheckSafeBrowsingStatus.DISABLED_BY_ADMIN,
        SafetyCheckSafeBrowsingStatus.DISABLED_BY_EXTENSION,
      ]),
    },
  },

  /** @private {?MetricsBrowserProxy} */
  metricsBrowserProxy_: null,

  /** @override */
  attached: function() {
    this.metricsBrowserProxy_ = MetricsBrowserProxyImpl.getInstance();

    // Register for safety check status updates.
    this.addWebUIListener(
        SafetyCheckCallbackConstants.SAFE_BROWSING_CHANGED,
        this.onSafetyCheckSafeBrowsingChanged_.bind(this));
  },

  /**
   * @param {!SafeBrowsingChangedEvent} event
   * @private
   */
  onSafetyCheckSafeBrowsingChanged_: function(event) {
    this.displayString_ = event.displayString;
    this.status_ = event.newState;
  },

  /**
   * @return {SafetyCheckIconStatus}
   * @private
   */
  getIconStatus_: function() {
    switch (this.status_) {
      case SafetyCheckSafeBrowsingStatus.CHECKING:
        return SafetyCheckIconStatus.RUNNING;
      case SafetyCheckSafeBrowsingStatus.ENABLED_STANDARD:
      case SafetyCheckSafeBrowsingStatus.ENABLED_ENHANCED:
      case SafetyCheckSafeBrowsingStatus.ENABLED_STANDARD_AVAILABLE_ENHANCED:
        return SafetyCheckIconStatus.SAFE;
      case SafetyCheckSafeBrowsingStatus.ENABLED:
        // ENABLED is deprecated.
        assertNotReached();
      case SafetyCheckSafeBrowsingStatus.DISABLED:
      case SafetyCheckSafeBrowsingStatus.DISABLED_BY_ADMIN:
      case SafetyCheckSafeBrowsingStatus.DISABLED_BY_EXTENSION:
        return SafetyCheckIconStatus.INFO;
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
      case SafetyCheckSafeBrowsingStatus.DISABLED:
        return this.i18n('safetyCheckSafeBrowsingButton');
      default:
        return null;
    }
  },

  /** @private */
  onButtonClick_: function() {
    // Log click both in action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
        SafetyCheckInteractions.SAFETY_CHECK_SAFE_BROWSING_MANAGE);
    this.metricsBrowserProxy_.recordAction(
        'Settings.SafetyCheck.ManageSafeBrowsing');
    this.openSecurityPage_();
  },

  /**
   * @private
   * @return {?string}
   */
  getManagedIcon_: function() {
    switch (this.status_) {
      case SafetyCheckSafeBrowsingStatus.DISABLED_BY_ADMIN:
        return 'cr20:domain';
      case SafetyCheckSafeBrowsingStatus.DISABLED_BY_EXTENSION:
        return 'cr:extension';
      default:
        return null;
    }
  },

  /**
   * @private
   * @return {?boolean}
   */
  isRowClickable_: function() {
    return this.rowClickableStatuses.has(this.status_);
  },

  /** @private */
  onRowClick_: function() {
    if (this.isRowClickable_()) {
      // Log click both in action and histogram.
      this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
          SafetyCheckInteractions
              .SAFETY_CHECK_SAFE_BROWSING_MANAGE_THROUGH_CARET_NAVIGATION);
      this.metricsBrowserProxy_.recordAction(
          'Settings.SafetyCheck.ManageSafeBrowsingThroughCaretNavigation');
      this.openSecurityPage_();
    }
  },

  /** @private */
  openSecurityPage_: function() {
    this.metricsBrowserProxy_.recordAction(
        'SafeBrowsing.Settings.ShowedFromSafetyCheck');
    Router.getInstance().navigateTo(
        routes.SECURITY, /* dynamicParams= */ null,
        /* removeSearch= */ true);
  },
});
