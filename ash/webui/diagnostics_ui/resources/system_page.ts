// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './battery_status_card.js';
import './cpu_card.js';
import './diagnostics_shared.css.js';
import './icons.html.js';
import './memory_card.js';
import './overview_card.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {StrictQueryMixin} from 'chrome://resources/ash/common/typescript_utils/strict_query_mixin.js';
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DiagnosticsBrowserProxyImpl} from './diagnostics_browser_proxy.js';
import {getSystemDataProvider} from './mojo_interface_provider.js';
import {OverviewCardElement} from './overview_card.js';
import {TestSuiteStatus} from './routine_list_executor.js';
import {SystemDataProviderInterface, SystemInfo} from './system_data_provider.mojom-webui.js';
import {getTemplate} from './system_page.html.js';

export interface SystemPageElement {
  $: {
    toast: CrToastElement,
  };
}

/**
 * @fileoverview
 * 'system-page' is the main page for viewing telemetric system information
 * and running diagnostic tests.
 */

const SystemPageElementBase = StrictQueryMixin(I18nMixin(PolymerElement));

export class SystemPageElement extends SystemPageElementBase {
  static get is(): string {
    return 'system-page';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      saveSessionLogEnabled_: {
        type: Boolean,
        value: true,
      },

      showBatteryStatusCard_: {
        type: Boolean,
        value: false,
      },

      testSuiteStatus: {
        type: Number,
        value: TestSuiteStatus.NOT_RUNNING,
      },

      systemInfoReceived_: {
        type: Boolean,
        value: false,
      },

      toastText_: {
        type: String,
        value: '',
      },

      isLoggedIn_: {
        type: Boolean,
        value: loadTimeData.getBoolean('isLoggedIn'),
      },

      isActive: {
        type: Boolean,
        value: true,
      },

    };
  }

  testSuiteStatus: TestSuiteStatus;
  isActive: boolean;
  protected systemInfoReceived_: boolean;
  protected saveSessionLogEnabled_: boolean;
  private showBatteryStatusCard_: boolean;
  private toastText_: string;
  private isLoggedIn_: boolean;
  private systemDataProvider_: SystemDataProviderInterface =
      getSystemDataProvider();
  private browserProxy_: DiagnosticsBrowserProxyImpl =
      DiagnosticsBrowserProxyImpl.getInstance();

  constructor() {
    super();
    this.fetchSystemInfo_();
    this.browserProxy_.initialize();
  }

  private fetchSystemInfo_(): void {
    this.systemDataProvider_.getSystemInfo().then((result) => {
      this.onSystemInfoReceived_(result.systemInfo);
    });
    setTimeout(() => this.recordLateSystemInfo_(), 3000);
  }

  private onSystemInfoReceived_(systemInfo: SystemInfo): void {
    this.systemInfoReceived_ = true;
    this.showBatteryStatusCard_ = systemInfo.deviceCapabilities.hasBattery;
  }

  private recordLateSystemInfo_(): void {
    if (!this.systemInfoReceived_) {
      console.warn('system info not received within three seconds.');
    }
  }

  protected onSessionLogClick_(): void {
    // Click already handled then leave early.
    if (!this.saveSessionLogEnabled_) {
      return;
    }

    this.saveSessionLogEnabled_ = false;
    this.browserProxy_.saveSessionLog()
        .then(
            /* @type {boolean} */ (success) => {
              const result = success ? 'Success' : 'Failure';
              this.toastText_ =
                  loadTimeData.getString(`sessionLogToastText${result}`);
              this.$.toast.show();
            })
        .catch(() => {/* File selection cancelled */})
        .finally(() => {
          this.saveSessionLogEnabled_ = true;
        });
  }

  /**
   * 'navigation-view-panel' is responsible for calling this function when
   * the active page changes.
   */
  onNavigationPageChanged({isActive}: {isActive: boolean}): void {
    this.isActive = isActive;
    if (isActive) {
      // Focus the topmost system page element.
      const overviewCard =
          this.strictQuery(OverviewCardElement.is, OverviewCardElement);
      const overviewCardContainer =
          overviewCard.strictQueryDiv('#overviewCardContainer');
      overviewCardContainer.focus();
      // TODO(ashleydp): Remove when a call can be made at a higher component
      // to avoid duplicate code in all navigatable pages.
      this.browserProxy_.recordNavigation('system');
    }
  }
}

customElements.define(SystemPageElement.is, SystemPageElement);
