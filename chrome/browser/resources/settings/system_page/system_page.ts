// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Settings that affect how Chrome interacts with the underlying
 * operating system (i.e. network, background processes, hardware).
 */

import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import '../controls/settings_toggle_button.js';
import '../relaunch_confirmation_dialog.js';
import '../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
// <if expr="_google_chrome and is_win">
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
// </if>
import {RelaunchMixin, RestartType} from '../relaunch_mixin.js';

import {getTemplate} from './system_page.html.js';
import {SystemPageBrowserProxyImpl} from './system_page_browser_proxy.js';


export interface SettingsSystemPageElement {
  $: {
    proxy: HTMLElement,
    hardwareAcceleration: SettingsToggleButtonElement,
  };
}

const SettingsSystemPageElementBase = RelaunchMixin(PolymerElement);

export class SettingsSystemPageElement extends SettingsSystemPageElementBase {
  static get is() {
    return 'settings-system-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      isProxyEnforcedByPolicy_: Boolean,
      isProxyDefault_: Boolean,
      // <if expr="chromeos_lacros">
      isSecondaryUser_: Boolean,
      // </if>

      // <if expr="_google_chrome and is_win">
      showFeatureNotificationsSetting_: {
        readOnly: true,
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showFeatureNotificationsSetting');
        },
      },
      // </if>
    };
  }

  static get observers() {
    return [
      'observeProxyPrefChanged_(prefs.proxy.*)',
    ];
  }

  constructor() {
    super();
    // <if expr="chromeos_lacros">
    this.isSecondaryUser_ = loadTimeData.getBoolean('isSecondaryUser');
    // </if>
  }

  prefs: {proxy: chrome.settingsPrivate.PrefObject};
  private isProxyEnforcedByPolicy_: boolean;
  private isProxyDefault_: boolean;
  // <if expr="chromeos_lacros">
  private isSecondaryUser_: boolean;
  // </if>
  // <if expr="_google_chrome and is_win">
  private showFeatureNotificationsSetting_: boolean;
  // </if>

  private observeProxyPrefChanged_() {
    const pref = this.prefs.proxy;
    // TODO(dbeam): do types of policy other than USER apply on ChromeOS?
    this.isProxyEnforcedByPolicy_ =
        pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED &&
        pref.controlledBy === chrome.settingsPrivate.ControlledBy.USER_POLICY;
    this.isProxyDefault_ = !this.isProxyEnforcedByPolicy_ && !pref.extensionId;
  }

  private onExtensionDisable_() {
    // TODO(dbeam): this is a pretty huge bummer. It means there are things
    // (inputs) that our prefs system is not observing. And that changes from
    // other sources (i.e. disabling/enabling an extension from
    // chrome://extensions or from the omnibox directly) will not update
    // |this.prefs.proxy| directly (nor the UI). We should fix this eventually.
    this.dispatchEvent(new CustomEvent(
        'refresh-pref', {bubbles: true, composed: true, detail: 'proxy'}));
  }

  private onProxyClick_() {
    if (this.isProxyDefault_) {
      SystemPageBrowserProxyImpl.getInstance().showProxySettings();
    }
  }

  private onRestartClick_(e: Event) {
    // Prevent event from bubbling up to the toggle button.
    e.stopPropagation();
    this.performRestart(RestartType.RESTART);
  }

  /**
   * @param enabled Whether hardware acceleration is currently enabled.
   */
  private shouldShowRestart_(enabled: boolean): boolean {
    const proxy = SystemPageBrowserProxyImpl.getInstance();
    return enabled !== proxy.wasHardwareAccelerationEnabledAtStartup();
  }

  // <if expr="_google_chrome and is_win">
  private onFeatureNotificationsChange_(e: Event) {
    const enabled = (e.target as SettingsToggleButtonElement).checked;
    MetricsBrowserProxyImpl.getInstance().recordFeatureNotificationsChange(
        enabled);
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-system-page': SettingsSystemPageElement;
  }
}

customElements.define(SettingsSystemPageElement.is, SettingsSystemPageElement);
