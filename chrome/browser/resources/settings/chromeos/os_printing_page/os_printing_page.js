// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared_css.js';
import './cups_printers.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

import {CupsPrinterInfo, CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl, CupsPrintersList, ManufacturersInfo, ModelsInfo, PrinterMakeModel, PrinterPpdMakeModel, PrinterSetupResult, PrintServerResult} from './cups_printers_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'os-settings-printing-page',

  behaviors: [
    DeepLinkingBehavior,
    RouteObserverBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** Printer search string. */
    searchTerm: {
      type: String,
    },

    /** @private {!Map<string, string>} */
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
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kPrintJobs,
        chromeos.settings.mojom.Setting.kScanningApp
      ]),
    },
  },

  /**
   * @param {!Route} route
   * @param {!Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.OS_PRINTING) {
      return;
    }

    this.attemptDeepLink();
  },

  /** @private */
  onTapCupsPrinters_() {
    Router.getInstance().navigateTo(routes.CUPS_PRINTERS);
  },

  /** @private */
  onOpenPrintManagement_() {
    CupsPrintersBrowserProxyImpl.getInstance().openPrintManagementApp();
  },

  /** @private */
  onOpenScanningApp_() {
    CupsPrintersBrowserProxyImpl.getInstance().openScanningApp();
    recordSettingChange(chromeos.settings.mojom.Setting.kScanningApp);
  }
});
