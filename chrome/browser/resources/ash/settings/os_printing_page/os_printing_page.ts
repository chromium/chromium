// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../os_settings_page/settings_card.js';
import '../settings_shared.css.js';
import './cups_printers.js';
import './cups_printers_browser_proxy.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {RouteOriginMixin} from '../route_origin_mixin.js';
import {Route, Router, routes} from '../router.js';

import {CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl} from './cups_printers_browser_proxy.js';
import {getTemplate} from './os_printing_page.html.js';

const OsSettingsPrintingPageElementBase =
    DeepLinkingMixin(RouteOriginMixin(PolymerElement));

export class OsSettingsPrintingPageElement extends
    OsSettingsPrintingPageElementBase {
  static get is() {
    return 'os-settings-printing-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       * */
      prefs: {
        type: Object,
        notify: true,
      },

      section_: {
        type: Number,
        value: Section.kPrinting,
        readOnly: true,
      },

      /**
       * Printer search string.
       * */
      searchTerm: {
        type: String,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () =>
            new Set<Setting>([Setting.kPrintJobs, Setting.kScanningApp]),
      },
    };
  }

  prefs: object;
  searchTerm: string;

  private browserProxy_: CupsPrintersBrowserProxy;
  private section_: Section;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = routes.OS_PRINTING;

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

  private onTapCupsPrinters_(): void {
    Router.getInstance().navigateTo(routes.CUPS_PRINTERS);
  }

  private onOpenPrintManagement_(): void {
    this.browserProxy_.openPrintManagementApp();
  }

  private onOpenScanningApp_(): void {
    this.browserProxy_.openScanningApp();
    recordSettingChange(Setting.kScanningApp);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsPrintingPageElement.is]: OsSettingsPrintingPageElement;
  }
}

customElements.define(
    OsSettingsPrintingPageElement.is, OsSettingsPrintingPageElement);
