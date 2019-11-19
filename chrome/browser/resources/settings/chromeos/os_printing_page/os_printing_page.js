// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'os-settings-printing-page',

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
      value: function() {
        const map = new Map();
        if (settings.routes.CUPS_PRINTERS) {
          map.set(settings.routes.CUPS_PRINTERS.path, '#cupsPrinters');
        }
        return map;
      },
    },
  },

  /** @private */
  onTapCupsPrinters_: function() {
    settings.navigateTo(settings.routes.CUPS_PRINTERS);
  },
});
