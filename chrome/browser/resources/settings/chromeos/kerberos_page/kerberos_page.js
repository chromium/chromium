// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-kerberos-page' is the settings page containing Kerberos Tickets
 * settings.
 */
Polymer({
  is: 'settings-kerberos-page',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        if (settings.routes.KERBEROS_ACCOUNTS_V2) {
          map.set(
              settings.routes.KERBEROS_ACCOUNTS_V2.path,
              '#kerberos-accounts-subpage-trigger');
        }
        return map;
      },
    },
  },

  /** @private */
  onKerberosAccountsTap_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.KERBEROS_ACCOUNTS_V2);
  },
});
