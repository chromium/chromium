// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Settings that affect how Chrome interacts with the underlying
 * operating system (i.e. network, background processes, hardware).
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import '../controls/settings_toggle_button.js';
import '../relaunch_confirmation_dialog.js';
import '../settings_page/settings_section.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
// <if expr="_google_chrome and is_win">
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
// </if>
import {RelaunchMixin, RestartType} from '../relaunch_mixin.js';
import {getSearchManager} from '../search_settings.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';

// <if expr="_google_chrome">
import type {OnDeviceAiBrowserProxy, OnDeviceAiEnabled} from './on_device_ai_browser_proxy.js';
import {OnDeviceAiBrowserProxyImpl} from './on_device_ai_browser_proxy.js';
// </if>
import {getTemplate} from './system_page.html.js';
import {SystemPageBrowserProxyImpl} from './system_page_browser_proxy.js';

interface ProxyOverrideRule {
  DestinationMatchers: string[];
  ProxyList: string[];
  ExcludeDestinationMatchers?: string[];
  Conditions?: Array<{
    DnsProbe: {
      Host: string,
      Result: string,
    },
  }>;
}

export interface SettingsSystemPageElement {
  $: {
    proxy: HTMLElement,
    proxyMultipleSources: HTMLElement,
    hardwareAcceleration: SettingsToggleButtonElement,
    onDeviceAiToggle: SettingsToggleButtonElement,
  };
}

const SettingsSystemPageElementBase =
    WebUiListenerMixin(PrefsMixin(RelaunchMixin(PolymerElement)));

export class SettingsSystemPageElement extends SettingsSystemPageElementBase
    implements SettingsPlugin {
  static get is() {
    return 'settings-system-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // <if expr="_google_chrome">
      showOnDeviceAiSettings_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showOnDeviceAiSettings'),
      },

      onDeviceAiPref_: {
        type: Object,
        value() {
          return {
            key: 'settings.on_device_ai_enabled',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: true,
          };
        },
      },
      // </if>

      isProxyEnforcedByPolicy_: Boolean,
      isProxyDefault_: Boolean,
      isProxyEnforcedByMultipleSources_: Boolean,

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
      'observeProxyPrefChanged_(prefs.proxy_override_rules.*)',
    ];
  }

  // <if expr="_google_chrome">
  declare private showOnDeviceAiSettings_: boolean;
  declare private onDeviceAiPref_: chrome.settingsPrivate.PrefObject<boolean>;
  private onDeviceAiBrowserProxy_: OnDeviceAiBrowserProxy =
      OnDeviceAiBrowserProxyImpl.getInstance();
  // </if>
  declare private isProxyEnforcedByPolicy_: boolean;
  declare private isProxyDefault_: boolean;
  declare private isProxyEnforcedByMultipleSources_: boolean;
  // <if expr="_google_chrome and is_win">
  declare private showFeatureNotificationsSetting_: boolean;
  // </if>

  // <if expr="_google_chrome">
  override ready() {
    super.ready();
    const setOnDeviceAiPref = (onDeviceAiEnabled: OnDeviceAiEnabled) =>
        this.setOnDeviceAiPref_(onDeviceAiEnabled);
    this.addWebUiListener('on-device-ai-enabled-changed', setOnDeviceAiPref);
    this.onDeviceAiBrowserProxy_.getOnDeviceAiEnabled().then(setOnDeviceAiPref);
  }
  // </if>

  private observeProxyPrefChanged_() {
    const pref = this.getPref('proxy');
    // TODO(dbeam): do types of policy other than USER apply on ChromeOS?
    this.isProxyEnforcedByPolicy_ =
        pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED &&
        pref.controlledBy === chrome.settingsPrivate.ControlledBy.USER_POLICY;
    this.isProxyDefault_ = !this.isProxyEnforcedByPolicy_ && !pref.extensionId;

    const rulesPref = this.getPref<ProxyOverrideRule[]>('proxy_override_rules');
    // Don't need to consider multiple source display when
    // `ProxyOverrideRules` preference is not set
    if (!rulesPref.value || rulesPref.value.length === 0) {
      this.isProxyEnforcedByMultipleSources_ = false;
      return;
    }

    // Don't need to consider multiple source display when proxy setting is not
    // set
    if (this.isProxyDefault_) {
      this.isProxyEnforcedByMultipleSources_ = true;
      return;
    }

    // When proxy settings and `ProxyOverrideRules` are from different levels of
    // sources
    if (pref.controlledBy !== rulesPref.controlledBy) {
      this.isProxyEnforcedByMultipleSources_ = true;
      return;
    }

    // When proxy settings and `ProxyOverrideRules` are both from policies, the
    // sources are considered to be the same
    if (pref.controlledBy === chrome.settingsPrivate.ControlledBy.USER_POLICY) {
      this.isProxyEnforcedByMultipleSources_ = false;
      return;
    }

    // When proxy settings and `ProxyOverrideRules` are both from extension(s),
    // the sources are considered to be the same only if they are set by the
    // same extension
    this.isProxyEnforcedByMultipleSources_ =
        (pref.extensionId !== rulesPref.extensionId);
  }

  private onExtensionDisable_() {
    // TODO(dbeam): this is a pretty huge bummer. It means there are things
    // (inputs) that our prefs system is not observing. And that changes from
    // other sources (i.e. disabling/enabling an extension from
    // chrome://extensions or from the omnibox directly) will not update
    // |this.getPref('proxy')| directly (nor the UI). We should fix this
    // eventually.
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

  // <if expr="_google_chrome">
  private onOnDeviceAiLearnMoreClicked_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('onDeviceAiLearnMoreUrl'));
  }

  private onOnDeviceAiToggleChange_(e: Event) {
    const enabled = (e.target as SettingsToggleButtonElement).checked;
    this.onDeviceAiBrowserProxy_.setOnDeviceAiEnabled(enabled);
  }

  private setOnDeviceAiPref_(onDeviceAiEnabled: OnDeviceAiEnabled) {
    const pref: chrome.settingsPrivate.PrefObject<boolean> = {
      key: 'settings.on_device_ai_enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: onDeviceAiEnabled.enabled,
    };

    if (!onDeviceAiEnabled.allowedByPolicy) {
      pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      pref.controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
      pref.value = false;
    }

    this.onDeviceAiPref_ = pref;
  }
  // </if>

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

  // SettingsPlugin implementation
  async searchContents(query: string) {
    const searchRequest = await getSearchManager().search(query, this);
    return searchRequest.getSearchResult();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-system-page': SettingsSystemPageElement;
  }
}

customElements.define(SettingsSystemPageElement.is, SettingsSystemPageElement);
