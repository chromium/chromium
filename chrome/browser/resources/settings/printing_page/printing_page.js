// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'settings-printing-page',

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    searchTerm: {
      type: String,
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        const map = new Map();
        if (settings.routes.CLOUD_PRINTERS) {
          map.set(settings.routes.CLOUD_PRINTERS.path, '#cloudPrinters');
        }
        // <if expr="chromeos">
        if (settings.routes.CUPS_PRINTERS) {
          map.set(settings.routes.CUPS_PRINTERS.path, '#cupsPrinters');
        }
        // </if>
        return map;
      },
    },

    // <if expr="chromeos">
    /**
     * TODO(crbug.com/950007): Remove when SplitSettings is the default because
     * CUPS printers will exist only in the OS settings page.
     * @private
     */
    showCupsPrinters_: {
      type: Boolean,
      value: () => loadTimeData.getBoolean('showOSSettings'),
    }
    // </if>
  },

  // <if expr="chromeos">
  /** @private */
  onTapCupsPrinters_: function() {
    settings.navigateTo(settings.routes.CUPS_PRINTERS);
  },
  // </if>

  // <if expr="not chromeos">
  onTapLocalPrinters_: function() {
    settings.PrintingBrowserProxyImpl.getInstance().openSystemPrintDialog();
  },
  // </if>

  /** @private */
  onTapCloudPrinters_: function() {
    settings.navigateTo(settings.routes.CLOUD_PRINTERS);
  },
});
