// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-default-browser-page' is the settings page that contains
 * settings to change the default browser (i.e. which the OS will open).
 */
Polymer({
  is: 'settings-default-browser-page',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /** @private */
    isDefault_: Boolean,

    /** @private */
    isSecondaryInstall_: Boolean,

    /** @private */
    isUnknownError_: Boolean,

    /** @private */
    maySetDefaultBrowser_: Boolean,
  },

  /** @private {settings.DefaultBrowserBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.DefaultBrowserBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready: function() {
    this.addWebUIListener(
        'browser-default-state-changed',
        this.updateDefaultBrowserState_.bind(this));

    this.browserProxy_.requestDefaultBrowserState().then(
        this.updateDefaultBrowserState_.bind(this));
  },

  /**
   * @param {!DefaultBrowserInfo} defaultBrowserState
   * @private
   */
  updateDefaultBrowserState_: function(defaultBrowserState) {
    this.isDefault_ = false;
    this.isSecondaryInstall_ = false;
    this.isUnknownError_ = false;
    this.maySetDefaultBrowser_ = false;

    if (defaultBrowserState.isDefault) {
      this.isDefault_ = true;
    } else if (!defaultBrowserState.canBeDefault) {
      this.isSecondaryInstall_ = true;
    } else if (
        !defaultBrowserState.isDisabledByPolicy &&
        !defaultBrowserState.isUnknownError) {
      this.maySetDefaultBrowser_ = true;
    } else {
      this.isUnknownError_ = true;
    }
  },

  /** @private */
  onSetDefaultBrowserTap_: function() {
    this.browserProxy_.setAsDefaultBrowser();
  },
});
