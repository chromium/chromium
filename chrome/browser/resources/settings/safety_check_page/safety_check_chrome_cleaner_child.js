// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-passwords-child' is the settings page containing the
 * safety check child showing the password status.
 */
import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ChromeCleanupProxy, ChromeCleanupProxyImpl} from '../chrome_cleanup_page/chrome_cleanup_proxy.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl, SafetyCheckInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {SafetyCheckCallbackConstants, SafetyCheckChromeCleanerStatus} from './safety_check_browser_proxy.js';
import {SafetyCheckIconStatus} from './safety_check_child.js';

/**
 * @typedef {{
 *   newState: SafetyCheckChromeCleanerStatus,
 *   displayString: string,
 * }}
 */
let ChromeCleanerChangedEvent;


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsSafetyCheckChromeCleanerChildElementBase =
    mixinBehaviors([I18nBehavior, WebUIListenerBehavior], PolymerElement);

/** @polymer */
export class SettingsSafetyCheckChromeCleanerChildElement extends
    SettingsSafetyCheckChromeCleanerChildElementBase {
  static get is() {
    return 'settings-safety-check-chrome-cleaner-child';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Current state of the safety check Chrome cleaner child.
       * @private {!SafetyCheckChromeCleanerStatus}
       */
      status_: {
        type: Number,
        value: SafetyCheckChromeCleanerStatus.HIDDEN,
      },

      /**
       * UI string to display for this child, received from the backend.
       * @private
       */
      displayString_: String,

      /**
       * A set of statuses that the entire row is clickable.
       * @type {!Set<!SafetyCheckChromeCleanerStatus>}
       * @private
       */
      rowClickableStatuses: {
        readOnly: true,
        type: Object,
        value: () => new Set([
          SafetyCheckChromeCleanerStatus.SCANNING_FOR_UWS,
          SafetyCheckChromeCleanerStatus.REMOVING_UWS,
          SafetyCheckChromeCleanerStatus.ERROR,
          SafetyCheckChromeCleanerStatus.NO_UWS_FOUND_WITH_TIMESTAMP,
          SafetyCheckChromeCleanerStatus.NO_UWS_FOUND_WITHOUT_TIMESTAMP,
        ]),
      },

    };
  }

  constructor() {
    super();

    /** @private {!ChromeCleanupProxy} */
    this.chromeCleanupBrowserProxy_ = ChromeCleanupProxyImpl.getInstance();

    /** @private {!MetricsBrowserProxy} */
    this.metricsBrowserProxy_ = MetricsBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    // Register for safety check status updates.
    this.addWebUIListener(
        SafetyCheckCallbackConstants.CHROME_CLEANER_CHANGED,
        this.onSafetyCheckChromeCleanerChanged_.bind(this));
  }

  /**
   * @param {!ChromeCleanerChangedEvent} event
   * @private
   */
  onSafetyCheckChromeCleanerChanged_(event) {
    this.status_ = event.newState;
    this.displayString_ = event.displayString;
  }


  /**
   * @return {boolean}
   * @private
   */
  showChild_() {
    return this.status_ !== SafetyCheckChromeCleanerStatus.HIDDEN &&
        loadTimeData.valueExists('safetyCheckChromeCleanerChildEnabled') &&
        loadTimeData.getBoolean('safetyCheckChromeCleanerChildEnabled');
  }

  /**
   * @return {SafetyCheckIconStatus}
   * @private
   */
  getIconStatus_() {
    switch (this.status_) {
      case SafetyCheckChromeCleanerStatus.HIDDEN:
      case SafetyCheckChromeCleanerStatus.CHECKING:
      case SafetyCheckChromeCleanerStatus.SCANNING_FOR_UWS:
      case SafetyCheckChromeCleanerStatus.REMOVING_UWS:
        return SafetyCheckIconStatus.RUNNING;
      case SafetyCheckChromeCleanerStatus.NO_UWS_FOUND_WITH_TIMESTAMP:
        return SafetyCheckIconStatus.SAFE;
      case SafetyCheckChromeCleanerStatus.REBOOT_REQUIRED:
      case SafetyCheckChromeCleanerStatus.DISABLED_BY_ADMIN:
      case SafetyCheckChromeCleanerStatus.ERROR:
      case SafetyCheckChromeCleanerStatus.NO_UWS_FOUND_WITHOUT_TIMESTAMP:
        return SafetyCheckIconStatus.INFO;
      case SafetyCheckChromeCleanerStatus.INFECTED:
        return SafetyCheckIconStatus.WARNING;
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
      case SafetyCheckChromeCleanerStatus.INFECTED:
        return this.i18n('safetyCheckReview');
      case SafetyCheckChromeCleanerStatus.REBOOT_REQUIRED:
        return this.i18n('chromeCleanupRestartButtonLabel');
      default:
        return null;
    }
  }

  /**
   * @private
   * @return {?string}
   */
  getButtonAriaLabel_() {
    switch (this.status_) {
      case SafetyCheckChromeCleanerStatus.INFECTED:
        return this.i18n('safetyCheckChromeCleanerButtonAriaLabel');
      case SafetyCheckChromeCleanerStatus.REBOOT_REQUIRED:
        return this.i18n('chromeCleanupRestartButtonLabel');
      default:
        return null;
    }
  }

  /**
   * @private
   * @return {string}
   */
  getButtonClass_() {
    switch (this.status_) {
      case SafetyCheckChromeCleanerStatus.INFECTED:
      case SafetyCheckChromeCleanerStatus.REBOOT_REQUIRED:
        return 'action-button';
      default:
        return '';
    }
  }

  /**
   * @param {!SafetyCheckInteractions} safetyCheckInteraction
   * @param {!string} userAction
   * @private
   */
  logUserInteraction_(safetyCheckInteraction, userAction) {
    // Log user interaction both in user action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
        safetyCheckInteraction);
    this.metricsBrowserProxy_.recordAction(userAction);
  }

  /** @private */
  onButtonClick_() {
    switch (this.status_) {
      case SafetyCheckChromeCleanerStatus.INFECTED:
        this.logUserInteraction_(
            SafetyCheckInteractions.CHROME_CLEANER_REVIEW_INFECTED_STATE,
            'Settings.SafetyCheck.ChromeCleanerReviewInfectedState');
        // Navigate to Chrome cleaner UI.
        this.navigateToFoilPage_();
        break;
      case SafetyCheckChromeCleanerStatus.REBOOT_REQUIRED:
        this.logUserInteraction_(
            SafetyCheckInteractions.CHROME_CLEANER_REBOOT,
            'Settings.SafetyCheck.ChromeCleanerReboot');
        this.chromeCleanupBrowserProxy_.restartComputer();
        break;
      default:
        // This is a state without an action.
        break;
    }
  }

  /**
   * @private
   * @return {?string}
   */
  getManagedIcon_() {
    switch (this.status_) {
      case SafetyCheckChromeCleanerStatus.DISABLED_BY_ADMIN:
        return 'cr20:domain';
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
      this.logUserInteraction_(
          SafetyCheckInteractions.CHROME_CLEANER_CARET_NAVIGATION,
          'Settings.SafetyCheck.ChromeCleanerCaretNavigation');
      this.navigateToFoilPage_();
    }
  }

  /** @private */
  navigateToFoilPage_() {
    Router.getInstance().navigateTo(
        routes.CHROME_CLEANUP,
        /* dynamicParams= */ null, /* removeSearch= */ true);
  }
}

customElements.define(
    SettingsSafetyCheckChromeCleanerChildElement.is,
    SettingsSafetyCheckChromeCleanerChildElement);
