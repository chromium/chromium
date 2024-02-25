// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-updates-child' is the settings page containing the safety
 * check child showing the browser's update status.
 */

// <if expr="not chromeos_ash">
import '../relaunch_confirmation_dialog.js';

// </if>

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SafetyCheckInteractions} from '../metrics_browser_proxy.js';
import {RelaunchMixin, RestartType} from '../relaunch_mixin.js';

import {SafetyCheckCallbackConstants, SafetyCheckUpdatesStatus} from './safety_check_browser_proxy.js';
import {SafetyCheckIconStatus} from './safety_check_child.js';
import {getTemplate} from './safety_check_updates_child.html.js';

interface UpdatesChangedEvent {
  newState: SafetyCheckUpdatesStatus;
  displayString: string;
}

const SettingsSafetyCheckUpdatesChildElementBase =
    RelaunchMixin(WebUiListenerMixin(I18nMixin(PolymerElement)));

export class SettingsSafetyCheckUpdatesChildElement extends
    SettingsSafetyCheckUpdatesChildElementBase {
  static get is() {
    return 'settings-safety-check-updates-child';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Current state of the safety check updates child.
       */
      status_: {
        type: Number,
        value: SafetyCheckUpdatesStatus.CHECKING,
      },

      /**
       * UI string to display for this child, received from the backend.
       */
      displayString_: String,
    };
  }

  private status_: SafetyCheckUpdatesStatus;
  private displayString_: string;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    // Register for safety check status updates.
    this.addWebUiListener(
        SafetyCheckCallbackConstants.UPDATES_CHANGED,
        this.onSafetyCheckUpdatesChanged_.bind(this));
  }

  private onSafetyCheckUpdatesChanged_(event: UpdatesChangedEvent) {
    this.status_ = event.newState;
    this.displayString_ = event.displayString;
  }

  private getIconStatus_(): SafetyCheckIconStatus {
    switch (this.status_) {
      case SafetyCheckUpdatesStatus.CHECKING:
      case SafetyCheckUpdatesStatus.UPDATING:
        return SafetyCheckIconStatus.RUNNING;
      case SafetyCheckUpdatesStatus.UPDATED:
        return SafetyCheckIconStatus.SAFE;
      case SafetyCheckUpdatesStatus.RELAUNCH:
      case SafetyCheckUpdatesStatus.DISABLED_BY_ADMIN:
      case SafetyCheckUpdatesStatus.UPDATE_TO_ROLLBACK_VERSION_DISALLOWED:
      case SafetyCheckUpdatesStatus.FAILED_OFFLINE:
      case SafetyCheckUpdatesStatus.UNKNOWN:
        return SafetyCheckIconStatus.INFO;
      case SafetyCheckUpdatesStatus.FAILED:
        return SafetyCheckIconStatus.WARNING;
      default:
        assertNotReached();
    }
  }

  private getButtonLabel_(): string|null {
    switch (this.status_) {
      case SafetyCheckUpdatesStatus.RELAUNCH:
        return this.i18n('aboutRelaunch');
      default:
        return null;
    }
  }

  private onButtonClick_() {
    // Log click both in action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
        SafetyCheckInteractions.UPDATES_RELAUNCH);
    this.metricsBrowserProxy_.recordAction(
        'Settings.SafetyCheck.RelaunchAfterUpdates');

    this.performRestart(RestartType.RELAUNCH);
  }

  private getManagedIcon_(): string|null {
    switch (this.status_) {
      case SafetyCheckUpdatesStatus.DISABLED_BY_ADMIN:
        return 'cr20:domain';
      default:
        return null;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-check-updates-child':
        SettingsSafetyCheckUpdatesChildElement;
  }
}

customElements.define(
    SettingsSafetyCheckUpdatesChildElement.is,
    SettingsSafetyCheckUpdatesChildElement);
