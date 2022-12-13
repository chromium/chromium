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
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DiagnosticsBrowserProxyImpl} from './diagnostics_browser_proxy.js';
import {ShowCautionBannerEvent} from './diagnostics_sticky_banner.js';
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
  static get is() {
    return 'system-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
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

      bannerMessage: {
        type: String,
        value: '',
      },

      scrollingClass_: {
        type: String,
        value: '',
      },

      scrollTimerId_: {
        type: Number,
        value: -1,
      },

      isActive: {
        type: Boolean,
        value: true,
      },

      isNetworkingEnabled: {
        type: Boolean,
        value: loadTimeData.getBoolean('isNetworkingEnabled'),
      },

    };
  }

  testSuiteStatus: TestSuiteStatus;
  bannerMessage: string;
  isActive: boolean;
  isNetworkingEnabled: boolean;
  protected systemInfoReceived_: boolean;
  protected saveSessionLogEnabled_: boolean;
  private showBatteryStatusCard_: boolean;
  private toastText_: string;
  private isLoggedIn_: boolean;
  private scrollingClass_: string;
  private scrollTimerId_: number;
  private systemDataProvider_: SystemDataProviderInterface =
      getSystemDataProvider();
  private browserProxy_: DiagnosticsBrowserProxyImpl =
      DiagnosticsBrowserProxyImpl.getInstance();

  constructor() {
    super();
    this.fetchSystemInfo_();
    this.browserProxy_.initialize();

    // Only use inner banner behavior if system page is in stand-alone mode.
    if (!this.isNetworkingEnabled) {
      this.addCautionBannerEventListeners_();
    }
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

  private addCautionBannerEventListeners_(): void {
    window.addEventListener('show-caution-banner', (e) => {
      const event = e as ShowCautionBannerEvent;
      assert(event.detail.message);
      this.bannerMessage = event.detail.message;
    });

    window.addEventListener('dismiss-caution-banner', () => {
      this.bannerMessage = '';
    });

    window.addEventListener('scroll', () => {
      if (!this.bannerMessage) {
        return;
      }

      // Reset timer since we've received another 'scroll' event.
      if (this.scrollTimerId_ !== -1) {
        this.scrollingClass_ = 'elevation-2';
        clearTimeout(this.scrollTimerId_);
      }

      // Remove box shadow from banner since the user has stopped scrolling
      // for at least 300ms.
      this.scrollTimerId_ =
          window.setTimeout(() => this.scrollingClass_ = '', 300);
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
      this.browserProxy_.recordNavigation('system');
    }
  }

  protected getCardContainerClass_(): string {
    const cardContainer = 'diagnostics-cards-container';
    return `${cardContainer}${this.isNetworkingEnabled ? '-nav' : ''}`;
  }
}

customElements.define(SystemPageElement.is, SystemPageElement);
