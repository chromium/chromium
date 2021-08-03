// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-updates-child' is the settings page containing the safety
 * check child showing the browser's update status.
 */
import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LifetimeBrowserProxyImpl} from '../lifetime_browser_proxy.js';
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


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsSafetyCheckUpdatesChildElementBase =
    mixinBehaviors([I18nBehavior, WebUIListenerBehavior], PolymerElement);

/** @polymer */
export class SettingsSafetyCheckUpdatesChildElement extends
    SettingsSafetyCheckUpdatesChildElementBase {
  static get is() {
    return 'settings-safety-check-updates-child';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
        SafetyCheckCallbackConstants.UPDATES_CHANGED,
        this.onSafetyCheckUpdatesChanged_.bind(this));
  }

  /**
   * @param {!UpdatesChangedEvent} event
   * @private
   */
  onSafetyCheckUpdatesChanged_(event) {
    this.status_ = event.newState;
    this.displayString_ = event.displayString;
  }

  /**
   * @return {SafetyCheckIconStatus}
   * @private
   */
  getIconStatus_() {
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
  }

  /**
   * @private
   * @return {?string}
   */
  getButtonLabel_() {
    switch (this.status_) {
      case SafetyCheckUpdatesStatus.RELAUNCH:
        return this.i18n('aboutRelaunch');
      default:
        return null;
    }
  }

  /** @private */
  onButtonClick_() {
    // Log click both in action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
        SafetyCheckInteractions.UPDATES_RELAUNCH);
    this.metricsBrowserProxy_.recordAction(
        'Settings.SafetyCheck.RelaunchAfterUpdates');

    LifetimeBrowserProxyImpl.getInstance().relaunch();
  }

  /**
   * @private
   * @return {?string}
   */
  getManagedIcon_() {
    switch (this.status_) {
      case SafetyCheckUpdatesStatus.DISABLED_BY_ADMIN:
        return 'cr20:domain';
      default:
        return null;
    }
  }
}

customElements.define(
    SettingsSafetyCheckUpdatesChildElement.is,
    SettingsSafetyCheckUpdatesChildElement);
