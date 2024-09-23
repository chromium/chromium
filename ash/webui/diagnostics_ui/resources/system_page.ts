// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './battery_status_card.js';
import './cpu_card.js';
import './diagnostics_shared.css.js';
import './icons.html.js';
import './memory_card.js';
import './overview_card.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {CrToastElement} from 'chrome://resources/ash/common/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
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

const SystemPageElementBase = I18nMixin(PolymerElement);

export class SystemPageElement extends SystemPageElementBase {
  static get is(): 'system-page' {
    return 'system-page' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      saveSessionLogEnabled: {
        type: Boolean,
        value: true,
      },

      showBatteryStatusCard: {
        type: Boolean,
        value: false,
      },

      testSuiteStatus: {
        type: Number,
        value: TestSuiteStatus.NOT_RUNNING,
      },

      systemInfoReceived: {
        type: Boolean,
        value: false,
      },

      toastText: {
        type: String,
        value: '',
      },

      isLoggedIn: {
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
  protected systemInfoReceived: boolean;
  protected saveSessionLogEnabled: boolean;
  private showBatteryStatusCard: boolean;
  private toastText: string;
  private isLoggedIn: boolean;
  private systemDataProvider: SystemDataProviderInterface =
      getSystemDataProvider();
  private browserProxy: DiagnosticsBrowserProxyImpl =
      DiagnosticsBrowserProxyImpl.getInstance();

  constructor() {
    super();
    this.fetchSystemInfo();
    this.browserProxy.initialize();
  }

  private fetchSystemInfo(): void {
    this.systemDataProvider.getSystemInfo().then((result) => {
      this.onSystemInfoReceived(result.systemInfo);
    });
    setTimeout(() => this.recordLateSystemInfo(), 3000);
  }

  private onSystemInfoReceived(systemInfo: SystemInfo): void {
    this.systemInfoReceived = true;
    this.showBatteryStatusCard = systemInfo.deviceCapabilities.hasBattery;
  }

  private recordLateSystemInfo(): void {
    if (!this.systemInfoReceived) {
      console.warn('system info not received within three seconds.');
    }
  }

  protected onSessionLogClick(): void {
    // Click already handled then leave early.
    if (!this.saveSessionLogEnabled) {
      return;
    }

    this.saveSessionLogEnabled = false;
    this.browserProxy.saveSessionLog()
        .then(
            /* @type {boolean} */ (success) => {
              const result = success ? 'Success' : 'Failure';
              this.toastText =
                  loadTimeData.getString(`sessionLogToastText${result}`);
              this.$.toast.show();
            })
        .catch(() => {/* File selection cancelled */})
        .finally(() => {
          this.saveSessionLogEnabled = true;
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
      const overviewCard: OverviewCardElement|null =
          this.shadowRoot!.querySelector('#overviewCard');
      assert(overviewCard);
      const overviewCardContainer: HTMLDivElement|null =
          overviewCard.shadowRoot!.querySelector('#overviewCardContainer');
      assert(overviewCardContainer);
      overviewCardContainer.focus();
      // TODO(ashleydp): Remove when a call can be made at a higher component
      // to avoid duplicate code in all navigatable pages.
      this.browserProxy.recordNavigation('system');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SystemPageElement.is]: SystemPageElement;
  }
}

customElements.define(SystemPageElement.is, SystemPageElement);
