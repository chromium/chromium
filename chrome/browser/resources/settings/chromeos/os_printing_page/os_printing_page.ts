// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../../settings_shared.css.js';
import './cups_printers.js';
import './cups_printers_browser_proxy.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {routes} from '../os_route.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, Router} from '../router.js';

import {CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl} from './cups_printers_browser_proxy.js';
import {getTemplate} from './os_printing_page.html.js';

const OsSettingsPrintingPageElementBase =
    DeepLinkingMixin(RouteObserverMixin(PolymerElement));

class OsSettingsPrintingPageElement extends OsSettingsPrintingPageElementBase {
  static get is(): string {
    return 'os-settings-printing-page';
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

      /**
       * Printer search string.
       * */
      searchTerm: {
        type: String,
      },

      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.CUPS_PRINTERS) {
            map.set(routes.CUPS_PRINTERS.path, '#cupsPrinters');
          }
          return map;
        },
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
  private focusConfig_: Map<string, string>;

  constructor() {
    super();

    this.browserProxy_ = CupsPrintersBrowserProxyImpl.getInstance();
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.OS_PRINTING) {
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
    'os-settings-printing-page': OsSettingsPrintingPageElement;
  }
}

customElements.define(
    OsSettingsPrintingPageElement.is, OsSettingsPrintingPageElement);
