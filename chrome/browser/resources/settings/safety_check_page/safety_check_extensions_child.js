// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-extensions-child' is the settings page containing the
 * safety check child showing the extension status.
 */
import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MetricsBrowserProxy, MetricsBrowserProxyImpl, SafetyCheckInteractions} from '../metrics_browser_proxy.js';
import {OpenWindowProxyImpl} from '../open_window_proxy.js';

import {SafetyCheckCallbackConstants, SafetyCheckExtensionsStatus} from './safety_check_browser_proxy.js';
import {SafetyCheckIconStatus} from './safety_check_child.js';

/**
 * @typedef {{
 *   newState: SafetyCheckExtensionsStatus,
 *   displayString: string,
 * }}
 */
let ExtensionssChangedEvent;

Polymer({
  is: 'settings-safety-check-extensions-child',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * Current state of the safety check extensions child.
     * @private {!SafetyCheckExtensionsStatus}
     */
    status_: {
      type: Number,
      value: SafetyCheckExtensionsStatus.CHECKING,
    },

    /**
     * UI string to display for this child, received from the backend.
     * @private
     */
    displayString_: String,

    /**
     * A set of statuses that the entire row is clickable.
     * @type {!Set<!SafetyCheckExtensionsStatus>}
     * @private
     */
    rowClickableStatuses: {
      readOnly: true,
      type: Object,
      value: () => new Set([
        SafetyCheckExtensionsStatus.NO_BLOCKLISTED_EXTENSIONS,
        SafetyCheckExtensionsStatus.ERROR,
        SafetyCheckExtensionsStatus.BLOCKLISTED_ALL_DISABLED,
        SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_ALL_BY_ADMIN,
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
        SafetyCheckCallbackConstants.EXTENSIONS_CHANGED,
        this.onSafetyCheckExtensionsChanged_.bind(this));
  },

  /**
   * @param {!ExtensionssChangedEvent} event
   * @private
   */
  onSafetyCheckExtensionsChanged_: function(event) {
    this.status_ = event.newState;
    this.displayString_ = event.displayString;
  },

  /**
   * @return {SafetyCheckIconStatus}
   * @private
   */
  getIconStatus_: function() {
    switch (this.status_) {
      case SafetyCheckExtensionsStatus.CHECKING:
        return SafetyCheckIconStatus.RUNNING;
      case SafetyCheckExtensionsStatus.ERROR:
      case SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_ALL_BY_ADMIN:
        return SafetyCheckIconStatus.INFO;
      case SafetyCheckExtensionsStatus.NO_BLOCKLISTED_EXTENSIONS:
      case SafetyCheckExtensionsStatus.BLOCKLISTED_ALL_DISABLED:
        return SafetyCheckIconStatus.SAFE;
      case SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_ALL_BY_USER:
      case SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_SOME_BY_USER:
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
      case SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_ALL_BY_USER:
      case SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_SOME_BY_USER:
        return this.i18n('safetyCheckReview');
      default:
        return null;
    }
  },

  /** @private */
  onButtonClick_: function() {
    // Log click both in action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
        SafetyCheckInteractions.SAFETY_CHECK_EXTENSIONS_REVIEW);
    this.metricsBrowserProxy_.recordAction(
        'Settings.SafetyCheck.ReviewExtensions');
    this.openExtensionsPage_();
  },

  /**
   * @private
   * @return {?string}
   */
  getManagedIcon_: function() {
    switch (this.status_) {
      case SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_ALL_BY_ADMIN:
        return 'cr20:domain';
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
              .SAFETY_CHECK_EXTENSIONS_REVIEW_THROUGH_CARET_NAVIGATION);
      this.metricsBrowserProxy_.recordAction(
          'Settings.SafetyCheck.ReviewExtensionsThroughCaretNavigation');
      this.openExtensionsPage_();
    }
  },

  /** @private */
  openExtensionsPage_: function() {
    OpenWindowProxyImpl.getInstance().openURL('chrome://extensions');
  },
});
