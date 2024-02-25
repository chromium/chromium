// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-extensions-child' is the settings page containing the
 * safety check child showing the extension status.
 */
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SafetyCheckInteractions} from '../metrics_browser_proxy.js';

import {SafetyCheckCallbackConstants, SafetyCheckExtensionsStatus} from './safety_check_browser_proxy.js';
import {SafetyCheckIconStatus} from './safety_check_child.js';
import {getTemplate} from './safety_check_extensions_child.html.js';

interface ExtensionsChangedEvent {
  newState: SafetyCheckExtensionsStatus;
  displayString: string;
}

const SettingsSafetyCheckExtensionsChildElementBase =
    WebUiListenerMixin(I18nMixin(PolymerElement));

export class SettingsSafetyCheckExtensionsChildElement extends
    SettingsSafetyCheckExtensionsChildElementBase {
  static get is() {
    return 'settings-safety-check-extensions-child';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Current state of the safety check extensions child.
       */
      status_: {
        type: Number,
        value: SafetyCheckExtensionsStatus.CHECKING,
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
          SafetyCheckExtensionsStatus.NO_BLOCKLISTED_EXTENSIONS,
          SafetyCheckExtensionsStatus.ERROR,
          SafetyCheckExtensionsStatus.BLOCKLISTED_ALL_DISABLED,
          SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_ALL_BY_ADMIN,
        ]),
      },
    };
  }

  private status_: SafetyCheckExtensionsStatus;
  private displayString_: string;
  private rowClickableStatuses: Set<SafetyCheckExtensionsStatus>;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    // Register for safety check status updates.
    this.addWebUiListener(
        SafetyCheckCallbackConstants.EXTENSIONS_CHANGED,
        this.onSafetyCheckExtensionsChanged_.bind(this));
  }

  private onSafetyCheckExtensionsChanged_(event: ExtensionsChangedEvent) {
    this.status_ = event.newState;
    this.displayString_ = event.displayString;
  }

  private getIconStatus_(): SafetyCheckIconStatus {
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
  }

  private getButtonLabel_(): string|null {
    switch (this.status_) {
      case SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_ALL_BY_USER:
      case SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_SOME_BY_USER:
        return this.i18n('safetyCheckReview');
      default:
        return null;
    }
  }

  private onButtonClick_() {
    // Log click both in action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
        SafetyCheckInteractions.EXTENSIONS_REVIEW);
    this.metricsBrowserProxy_.recordAction(
        'Settings.SafetyCheck.ReviewExtensions');
    this.openExtensionsPage_();
  }

  private getManagedIcon_(): string|null {
    switch (this.status_) {
      case SafetyCheckExtensionsStatus.BLOCKLISTED_REENABLED_ALL_BY_ADMIN:
        return 'cr20:domain';
      default:
        return null;
    }
  }

  private isRowClickable_(): boolean {
    return this.rowClickableStatuses.has(this.status_);
  }

  private onRowClick_() {
    if (this.isRowClickable_()) {
      // Log click both in action and histogram.
      this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
          SafetyCheckInteractions.EXTENSIONS_CARET_NAVIGATION);
      this.metricsBrowserProxy_.recordAction(
          'Settings.SafetyCheck.ReviewExtensionsThroughCaretNavigation');
      this.openExtensionsPage_();
    }
  }

  private openExtensionsPage_() {
    OpenWindowProxyImpl.getInstance().openUrl('chrome://extensions');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-check-extensions-child':
        SettingsSafetyCheckExtensionsChildElement;
  }
}

customElements.define(
    SettingsSafetyCheckExtensionsChildElement.is,
    SettingsSafetyCheckExtensionsChildElement);
