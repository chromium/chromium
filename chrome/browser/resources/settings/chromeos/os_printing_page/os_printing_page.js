// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../settings_page/settings_animated_pages.js';
import '../../settings_page/settings_subpage.js';
import '../../settings_shared.css.js';
import './cups_printers.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl} from './cups_printers_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const OsSettingsPrintingPageElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      RouteObserverBehavior,
    ],
    PolymerElement);

/** @polymer */
class OsSettingsPrintingPageElement extends OsSettingsPrintingPageElementBase {
  static get is() {
    return 'os-settings-printing-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([Setting.kPrintJobs, Setting.kScanningApp]),
      },
    };
  }

  constructor() {
    super();

    /** @private {!CupsPrintersBrowserProxy} */
    this.browserProxy_ = CupsPrintersBrowserProxyImpl.getInstance();
  }

  /**
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.OS_PRINTING) {
      return;
    }

    this.attemptDeepLink();
  }

  /** @private */
  onTapCupsPrinters_() {
    Router.getInstance().navigateTo(routes.CUPS_PRINTERS);
  }

  /** @private */
  onOpenPrintManagement_() {
    this.browserProxy_.openPrintManagementApp();
  }

  /** @private */
  onOpenScanningApp_() {
    this.browserProxy_.openScanningApp();
    recordSettingChange(Setting.kScanningApp);
  }
}

customElements.define(
    OsSettingsPrintingPageElement.is, OsSettingsPrintingPageElement);
