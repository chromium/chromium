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
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl, PasswordManagerProxy} from '../autofill_page/password_manager_proxy.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl, SafetyCheckInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {SafetyCheckCallbackConstants, SafetyCheckPasswordsStatus} from './safety_check_browser_proxy.js';
import {SafetyCheckIconStatus} from './safety_check_child.js';

/**
 * @typedef {{
 *   newState: SafetyCheckPasswordsStatus,
 *   displayString: string,
 * }}
 */
let PasswordsChangedEvent;


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsSafetyCheckPasswordsChildElementBase =
    mixinBehaviors([I18nBehavior, WebUIListenerBehavior], PolymerElement);

/** @polymer */
export class SettingsSafetyCheckPasswordsChildElement extends
    SettingsSafetyCheckPasswordsChildElementBase {
  static get is() {
    return 'settings-safety-check-passwords-child';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Current state of the safety check passwords child.
       * @private {!SafetyCheckPasswordsStatus}
       */
      status_: {
        type: Number,
        value: SafetyCheckPasswordsStatus.CHECKING,
      },

      /**
       * UI string to display for this child, received from the backend.
       * @private
       */
      displayString_: String,

      /**
       * A set of statuses that the entire row is clickable.
       * @type {!Set<!SafetyCheckPasswordsStatus>}
       * @private
       */
      rowClickableStatuses: {
        readOnly: true,
        type: Object,
        value: () => new Set([
          SafetyCheckPasswordsStatus.SAFE,
          SafetyCheckPasswordsStatus.QUOTA_LIMIT,
          SafetyCheckPasswordsStatus.ERROR,
          SafetyCheckPasswordsStatus.WEAK_PASSWORDS_EXIST,
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
        SafetyCheckCallbackConstants.PASSWORDS_CHANGED,
        this.onSafetyCheckPasswordsChanged_.bind(this));
  }

  /**
   * @param {!PasswordsChangedEvent} event
   * @private
   */
  onSafetyCheckPasswordsChanged_(event) {
    this.status_ = event.newState;
    this.displayString_ = event.displayString;
  }

  /**
   * @return {SafetyCheckIconStatus}
   * @private
   */
  getIconStatus_() {
    switch (this.status_) {
      case SafetyCheckPasswordsStatus.CHECKING:
        return SafetyCheckIconStatus.RUNNING;
      case SafetyCheckPasswordsStatus.SAFE:
        return SafetyCheckIconStatus.SAFE;
      case SafetyCheckPasswordsStatus.COMPROMISED:
        return SafetyCheckIconStatus.WARNING;
      case SafetyCheckPasswordsStatus.OFFLINE:
      case SafetyCheckPasswordsStatus.NO_PASSWORDS:
      case SafetyCheckPasswordsStatus.SIGNED_OUT:
      case SafetyCheckPasswordsStatus.QUOTA_LIMIT:
      case SafetyCheckPasswordsStatus.ERROR:
      case SafetyCheckPasswordsStatus.FEATURE_UNAVAILABLE:
      case SafetyCheckPasswordsStatus.WEAK_PASSWORDS_EXIST:
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
      case SafetyCheckPasswordsStatus.COMPROMISED:
        return this.i18n('safetyCheckReview');
      default:
        return null;
    }
  }

  /** @private */
  onButtonClick_() {
    // Log click both in action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
        SafetyCheckInteractions.PASSWORDS_MANAGE_COMPROMISED_PASSWORDS);
    this.metricsBrowserProxy_.recordAction(
        'Settings.SafetyCheck.ManagePasswords');
    this.openPasswordCheckPage_();
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
          this.status_ === SafetyCheckPasswordsStatus.WEAK_PASSWORDS_EXIST ?
              SafetyCheckInteractions.PASSWORDS_MANAGE_WEAK_PASSWORDS :
              SafetyCheckInteractions.PASSWORDS_CARET_NAVIGATION);
      this.metricsBrowserProxy_.recordAction(
          this.status_ === SafetyCheckPasswordsStatus.WEAK_PASSWORDS_EXIST ?
              'Settings.SafetyCheck.ManageWeakPasswords' :
              'Settings.SafetyCheck.ManagePasswordsThroughCaretNavigation');
      this.openPasswordCheckPage_();
    }
  }

  /** @private */
  openPasswordCheckPage_() {
    Router.getInstance().navigateTo(
        routes.CHECK_PASSWORDS,
        /* dynamicParams= */ null, /* removeSearch= */ true);
    PasswordManagerImpl.getInstance().recordPasswordCheckReferrer(
        PasswordManagerProxy.PasswordCheckReferrer.SAFETY_CHECK);
  }
}

customElements.define(
    SettingsSafetyCheckPasswordsChildElement.is,
    SettingsSafetyCheckPasswordsChildElement);
