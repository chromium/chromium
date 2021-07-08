// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-safe-browsing-child' is the settings page containing the
 * safety check child showing the Safe Browsing status.
 */
import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MetricsBrowserProxy, MetricsBrowserProxyImpl, SafetyCheckInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {SafetyCheckCallbackConstants, SafetyCheckSafeBrowsingStatus} from './safety_check_browser_proxy.js';
import {SafetyCheckIconStatus} from './safety_check_child.js';

/**
 * @typedef {{
 *   newState: SafetyCheckSafeBrowsingStatus,
 *   displayString: string,
 * }}
 */
let SafeBrowsingChangedEvent;


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsSafetyCheckSafeBrowsingChildElementBase =
    mixinBehaviors([I18nBehavior, WebUIListenerBehavior], PolymerElement);

/** @polymer */
export class SettingsSafetyCheckSafeBrowsingChildElement extends
    SettingsSafetyCheckSafeBrowsingChildElementBase {
  static get is() {
    return 'settings-safety-check-safe-browsing-child';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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

    };
  }

  constructor() {
    super();

    /** @private {!MetricsBrowserProxy} */
    this.metricsBrowserProxy_ = MetricsBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    // Register for safety check status updates.
    this.addWebUIListener(
        SafetyCheckCallbackConstants.SAFE_BROWSING_CHANGED,
        this.onSafetyCheckSafeBrowsingChanged_.bind(this));
  }

  /**
   * @param {!SafeBrowsingChangedEvent} event
   * @private
   */
  onSafetyCheckSafeBrowsingChanged_(event) {
    this.displayString_ = event.displayString;
    this.status_ = event.newState;
  }

  /**
   * @return {SafetyCheckIconStatus}
   * @private
   */
  getIconStatus_() {
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
  }

  /**
   * @private
   * @return {?string}
   */
  getButtonLabel_() {
    switch (this.status_) {
      case SafetyCheckSafeBrowsingStatus.DISABLED:
        return this.i18n('safetyCheckSafeBrowsingButton');
      default:
        return null;
    }
  }

  /** @private */
  onButtonClick_() {
    // Log click both in action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
        SafetyCheckInteractions.SAFE_BROWSING_MANAGE);
    this.metricsBrowserProxy_.recordAction(
        'Settings.SafetyCheck.ManageSafeBrowsing');
    this.openSecurityPage_();
  }

  /**
   * @private
   * @return {?string}
   */
  getManagedIcon_() {
    switch (this.status_) {
      case SafetyCheckSafeBrowsingStatus.DISABLED_BY_ADMIN:
        return 'cr20:domain';
      case SafetyCheckSafeBrowsingStatus.DISABLED_BY_EXTENSION:
        return 'cr:extension';
      default:
        return null;
    }
  }

  /**
   * @private
   * @return {?boolean}
   */
  isRowClickable_() {
    return this.rowClickableStatuses.has(this.status_);
  }

  /** @private */
  onRowClick_() {
    if (this.isRowClickable_()) {
      // Log click both in action and histogram.
      this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
          SafetyCheckInteractions.SAFE_BROWSING_CARET_NAVIGATION);
      this.metricsBrowserProxy_.recordAction(
          'Settings.SafetyCheck.ManageSafeBrowsingThroughCaretNavigation');
      this.openSecurityPage_();
    }
  }

  /** @private */
  openSecurityPage_() {
    this.metricsBrowserProxy_.recordAction(
        'SafeBrowsing.Settings.ShowedFromSafetyCheck');
    Router.getInstance().navigateTo(
        routes.SECURITY, /* dynamicParams= */ null,
        /* removeSearch= */ true);
  }
}

customElements.define(
    SettingsSafetyCheckSafeBrowsingChildElement.is,
    SettingsSafetyCheckSafeBrowsingChildElement);
