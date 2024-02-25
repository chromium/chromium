// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-passwords-child' is the settings page containing the
 * safety check child showing the password status.
 */
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordCheckReferrer, PasswordManagerImpl, PasswordManagerPage} from '../autofill_page/password_manager_proxy.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SafetyCheckInteractions} from '../metrics_browser_proxy.js';

import {SafetyCheckCallbackConstants, SafetyCheckPasswordsStatus} from './safety_check_browser_proxy.js';
import {SafetyCheckIconStatus} from './safety_check_child.js';
import {getTemplate} from './safety_check_passwords_child.html.js';

interface PasswordsChangedEvent {
  newState: SafetyCheckPasswordsStatus;
  displayString: string;
}

const SettingsSafetyCheckPasswordsChildElementBase =
    WebUiListenerMixin(I18nMixin(PolymerElement));

export class SettingsSafetyCheckPasswordsChildElement extends
    SettingsSafetyCheckPasswordsChildElementBase {
  static get is() {
    return 'settings-safety-check-passwords-child';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Current state of the safety check passwords child.
       */
      status_: {
        type: Number,
        value: SafetyCheckPasswordsStatus.CHECKING,
      },

      /**
       * UI string to display for this child, received from the backend.
       */
      displayString_: String,

      /**
       * A set of statuses that the entire row is clickable.
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

  private status_: SafetyCheckPasswordsStatus;
  private displayString_: string;
  private rowClickableStatuses: Set<SafetyCheckPasswordsStatus>;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    // Register for safety check status updates.
    this.addWebUiListener(
        SafetyCheckCallbackConstants.PASSWORDS_CHANGED,
        this.onSafetyCheckPasswordsChanged_.bind(this));
  }

  private onSafetyCheckPasswordsChanged_(event: PasswordsChangedEvent) {
    this.status_ = event.newState;
    this.displayString_ = event.displayString;
  }

  private getIconStatus_(): SafetyCheckIconStatus {
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
      case SafetyCheckPasswordsStatus.REUSED_PASSWORDS_EXIST:
      case SafetyCheckPasswordsStatus.MUTED_COMPROMISED_EXIST:
        return SafetyCheckIconStatus.INFO;
      default:
        assertNotReached();
    }
  }

  private getButtonLabel_(): string|null {
    switch (this.status_) {
      case SafetyCheckPasswordsStatus.COMPROMISED:
        return this.i18n('safetyCheckReview');
      default:
        return null;
    }
  }

  private onButtonClick_() {
    // Log click both in action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
        SafetyCheckInteractions.PASSWORDS_MANAGE_COMPROMISED_PASSWORDS);
    this.metricsBrowserProxy_.recordAction(
        'Settings.SafetyCheck.ManagePasswords');
    this.openPasswordCheckPage_();
  }

  private isRowClickable_(): boolean {
    return this.rowClickableStatuses.has(this.status_);
  }

  private onRowClick_() {
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

  private openPasswordCheckPage_() {
    PasswordManagerImpl.getInstance().recordPasswordCheckReferrer(
        PasswordCheckReferrer.SAFETY_CHECK);
    PasswordManagerImpl.getInstance().showPasswordManager(
        PasswordManagerPage.CHECKUP);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-check-passwords-child':
        SettingsSafetyCheckPasswordsChildElement;
  }
}

customElements.define(
    SettingsSafetyCheckPasswordsChildElement.is,
    SettingsSafetyCheckPasswordsChildElement);
