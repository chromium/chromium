// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'printing-settings-card' is the card element containing printing settings.
 */

import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';
import './cups_printers_browser_proxy.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl} from './cups_printers_browser_proxy.js';
import {getTemplate} from './printing_settings_card.html.js';

const PrintingSettingsCardElementBase =
    DeepLinkingMixin(RouteOriginMixin(I18nMixin(PolymerElement)));

export class PrintingSettingsCardElement extends
    PrintingSettingsCardElementBase {
  static get is() {
    return 'printing-settings-card' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () =>
            new Set<Setting>([Setting.kPrintJobs, Setting.kScanningApp]),
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value: () => {
          return isRevampWayfindingEnabled();
        },
      },

      rowIcons_: {
        type: Object,
        value() {
          if (isRevampWayfindingEnabled()) {
            return {
              print: 'os-settings:device-print',
              scan: 'os-settings:device-scan',
            };
          }

          return {
            print: '',
            scan: '',
          };
        },
      },
    };
  }

  private browserProxy_: CupsPrintersBrowserProxy;
  private isRevampWayfindingEnabled_: boolean;
  private rowIcons_: Record<string, string>;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route =
        this.isRevampWayfindingEnabled_ ? routes.DEVICE : routes.OS_PRINTING;


    this.browserProxy_ = CupsPrintersBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();

    this.addFocusConfig(routes.CUPS_PRINTERS, '#cupsPrintersRow');
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
    super.currentRouteChanged(newRoute, oldRoute);

    // Does not apply to this page.
    if (newRoute !== this.route) {
      return;
    }

    this.attemptDeepLink();
  }

  private onClickCupsPrint_(): void {
    Router.getInstance().navigateTo(routes.CUPS_PRINTERS);
  }

  private onClickPrintManagement_(): void {
    this.browserProxy_.openPrintManagementApp();
  }

  private onClickScanningApp_(): void {
    this.browserProxy_.openScanningApp();
    recordSettingChange(Setting.kScanningApp);
  }

  private getCupsPrintDescription_(): string {
    if (this.isRevampWayfindingEnabled_) {
      return this.i18n('cupsPrintDescription');
    }
    return '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PrintingSettingsCardElement.is]: PrintingSettingsCardElement;
  }
}

customElements.define(
    PrintingSettingsCardElement.is, PrintingSettingsCardElement);
