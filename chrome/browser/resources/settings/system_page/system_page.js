// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Settings that affect how Chrome interacts with the underlying
 * operating system (i.e. network, background processes, hardware).
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/extension_controlled_indicator.js';
import '../controls/settings_toggle_button.js';
import '../prefs/prefs.js';
import '../settings_shared_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LifetimeBrowserProxyImpl} from '../lifetime_browser_proxy.js';

import {SystemPageBrowserProxyImpl} from './system_page_browser_proxy.js';

Polymer({
  is: 'settings-system-page',

  _template: html`{__html_template__}`,

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
  observeProxyPrefChanged_() {
    /** @type {!chrome.settingsPrivate.PrefObject} */
    const pref = this.prefs.proxy;
    // TODO(dbeam): do types of policy other than USER apply on ChromeOS?
    this.isProxyEnforcedByPolicy_ =
        pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED &&
        pref.controlledBy === chrome.settingsPrivate.ControlledBy.USER_POLICY;
    this.isProxyDefault_ = !this.isProxyEnforcedByPolicy_ && !pref.extensionId;
  },

  /** @private */
  onExtensionDisable_() {
    // TODO(dbeam): this is a pretty huge bummer. It means there are things
    // (inputs) that our prefs system is not observing. And that changes from
    // other sources (i.e. disabling/enabling an extension from
    // chrome://extensions or from the omnibox directly) will not update
    // |this.prefs.proxy| directly (nor the UI). We should fix this eventually.
    this.fire('refresh-pref', 'proxy');
  },

  /** @private */
  onProxyTap_() {
    if (this.isProxyDefault_) {
      SystemPageBrowserProxyImpl.getInstance().showProxySettings();
    }
  },

  /** @private */
  onRestartTap_(e) {
    // Prevent event from bubbling up to the toggle button.
    e.stopPropagation();
    // TODO(dbeam): we should prompt before restarting the browser.
    LifetimeBrowserProxyImpl.getInstance().restart();
  },

  /**
   * @param {boolean} enabled Whether hardware acceleration is currently
   *     enabled.
   * @private
   */
  shouldShowRestart_(enabled) {
    const proxy = SystemPageBrowserProxyImpl.getInstance();
    return enabled !== proxy.wasHardwareAccelerationEnabledAtStartup();
  },
});
