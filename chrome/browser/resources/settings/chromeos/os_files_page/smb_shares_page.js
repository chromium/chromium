// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'settings-smb-shares-page',

  behaviors: [
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    showAddSmbDialog_: Boolean,
  },

  /**
   * Overridden from settings.RouteObserverBehavior.
   * @param {!settings.Route} route
   * @protected
   */
  currentRouteChanged: function(route) {
    if (route == settings.routes.SMB_SHARES) {
      this.showAddSmbDialog_ =
          settings.getQueryParameters().get('showAddShare') == 'true';
    }
  },

  /** @private */
  onAddShareTap_: function() {
    this.showAddSmbDialog_ = true;
  },

  /** @private */
  onAddSmbDialogClosed_: function() {
    this.showAddSmbDialog_ = false;
  },
});
