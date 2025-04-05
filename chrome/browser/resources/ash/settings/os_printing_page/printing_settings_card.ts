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
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import type {Route} from '../router.js';
import {Router, routes} from '../router.js';

import type {CupsPrintersBrowserProxy} from './cups_printers_browser_proxy.js';
import {CupsPrintersBrowserProxyImpl} from './cups_printers_browser_proxy.js';
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

  // DeepLinkingMixin override
  override supportedSettingIds = new Set<Setting>([
    Setting.kPrintJobs,
    Setting.kScanningApp,
  ]);

  private browserProxy_: CupsPrintersBrowserProxy;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = routes.DEVICE;


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
}

declare global {
  interface HTMLElementTagNameMap {
    [PrintingSettingsCardElement.is]: PrintingSettingsCardElement;
  }
}

customElements.define(
    PrintingSettingsCardElement.is, PrintingSettingsCardElement);
