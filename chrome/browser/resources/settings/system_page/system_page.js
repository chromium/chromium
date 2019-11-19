// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Settings that affect how Chrome interacts with the underlying
 * operating system (i.e. network, background processes, hardware).
 */

Polymer({
  is: 'settings-system-page',

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    isProxyEnforcedByPolicy_: {
      type: Boolean,
    },

    /** @private */
    isProxyDefault_: {
      type: Boolean,
    },
  },

  observers: [
    'observeProxyPrefChanged_(prefs.proxy.*)',
  ],

  /**
   * @private
   */
  observeProxyPrefChanged_: function() {
    const pref = this.get('prefs.proxy');
    // TODO(dbeam): do types of policy other than USER apply on ChromeOS?
    this.isProxyEnforcedByPolicy_ =
        pref.enforcement == chrome.settingsPrivate.Enforcement.ENFORCED &&
        pref.controlledBy == chrome.settingsPrivate.ControlledBy.USER_POLICY;
    this.isProxyDefault_ = !this.isProxyEnforcedByPolicy_ && !pref.extensionId;
  },

  /** @private */
  onExtensionDisable_: function() {
    // TODO(dbeam): this is a pretty huge bummer. It means there are things
    // (inputs) that our prefs system is not observing. And that changes from
    // other sources (i.e. disabling/enabling an extension from
    // chrome://extensions or from the omnibox directly) will not update
    // |this.prefs.proxy| directly (nor the UI). We should fix this eventually.
    this.fire('refresh-pref', 'proxy');
  },

  /** @private */
  onProxyTap_: function() {
    if (this.isProxyDefault_) {
      settings.SystemPageBrowserProxyImpl.getInstance().showProxySettings();
    }
  },

  /** @private */
  onRestartTap_: function(e) {
    // Prevent event from bubbling up to the toggle button.
    e.stopPropagation();
    // TODO(dbeam): we should prompt before restarting the browser.
    settings.LifetimeBrowserProxyImpl.getInstance().restart();
  },

  /**
   * @param {boolean} enabled Whether hardware acceleration is currently
   *     enabled.
   * @private
   */
  shouldShowRestart_: function(enabled) {
    const proxy = settings.SystemPageBrowserProxyImpl.getInstance();
    return enabled != proxy.wasHardwareAccelerationEnabledAtStartup();
  },
});
