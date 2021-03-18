// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'os-settings-printing-page',

  behaviors: [
    DeepLinkingBehavior,
    settings.RouteObserverBehavior,
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
        if (settings.routes.CUPS_PRINTERS) {
          map.set(settings.routes.CUPS_PRINTERS.path, '#cupsPrinters');
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
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.OS_PRINTING) {
      return;
    }

    this.attemptDeepLink();
  },

  /** @private */
  onTapCupsPrinters_() {
    settings.Router.getInstance().navigateTo(settings.routes.CUPS_PRINTERS);
  },

  /** @private */
  onOpenPrintManagement_() {
    settings.CupsPrintersBrowserProxyImpl.getInstance()
        .openPrintManagementApp();
  },

  /** @private */
  onOpenScanningApp_() {
    settings.CupsPrintersBrowserProxyImpl.getInstance().openScanningApp();
    settings.recordSettingChange(chromeos.settings.mojom.Setting.kScanningApp);
  }
});
