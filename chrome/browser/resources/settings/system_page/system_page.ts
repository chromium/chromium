// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Settings that affect how Chrome interacts with the underlying
 * operating system (i.e. network, background processes, hardware).
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
// <if expr="_google_chrome">
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
// </if>
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import '/shared/settings/controls/cr_policy_pref_indicator.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import '../controls/settings_toggle_button.js';
import '../relaunch_confirmation_dialog.js';
import '../settings_page/settings_section.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
// <if expr="_google_chrome and is_win">
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
// </if>
import {RelaunchMixinLit, RestartType} from '../relaunch_mixin_lit.js';
// <if expr="_google_chrome">
import {routes} from '../route.js';
import {Router} from '../router.js';
// </if>
import {getSearchManager} from '../search_settings.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';

import {getCss} from './system_page.css.js';
import {getHtml} from './system_page.html.js';
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
  };
}

const SettingsSystemPageElementBase = WebUiListenerMixinLit(
    PrefServiceObserverMixinLit(RelaunchMixinLit(CrLitElement)));

export class SettingsSystemPageElement extends SettingsSystemPageElementBase
    implements SettingsPlugin {
  static get is() {
    return 'settings-system-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isProxyEnforcedByPolicy_: {type: Boolean},
      isProxyDefault_: {type: Boolean},
      isProxyEnforcedByMultipleSources_: {type: Boolean},

      // <if expr="_google_chrome and is_win">
      showFeatureNotificationsSetting_: {type: Boolean},
      // </if>
      // <if expr="is_win">
      showProcessIsolationSetting_: {type: Boolean},
      // </if>

      proxyPref_: {type: Object},
      proxyOverrideRulesPref_: {type: Object},
      hardwareAccelerationModeEnabledPref_: {type: Object},
      // <if expr="is_win">
      isolationStateEnabledPref_: {type: Object},
      // </if>
    };
  }

  protected accessor isProxyEnforcedByPolicy_: boolean = false;
  protected accessor isProxyDefault_: boolean = false;
  protected accessor isProxyEnforcedByMultipleSources_: boolean = false;

  // <if expr="_google_chrome and is_win">
  protected accessor showFeatureNotificationsSetting_: boolean =
      loadTimeData.getBoolean('showFeatureNotificationsSetting');
  // </if>
  // <if expr="is_win">
  protected accessor showProcessIsolationSetting_: boolean =
      loadTimeData.getBoolean('showProcessIsolationSetting');
  private processIsolationEnabledAtStartup_: boolean|undefined;
  // </if>

  protected accessor proxyPref_: chrome.settingsPrivate.PrefObject<unknown>|
      undefined;
  protected accessor proxyOverrideRulesPref_:
      chrome.settingsPrivate.PrefObject<ProxyOverrideRule[]>|undefined;
  protected accessor hardwareAccelerationModeEnabledPref_:
      chrome.settingsPrivate.PrefObject<boolean>|undefined;
  // <if expr="is_win">
  protected accessor isolationStateEnabledPref_:
      chrome.settingsPrivate.PrefObject<boolean>|undefined;
  // </if>

  override connectedCallback() {
    super.connectedCallback();

    this.mirrorPrefs({
      'proxy': 'proxyPref_',
      'proxy_override_rules': 'proxyOverrideRulesPref_',
      'hardware_acceleration_mode.enabled':
          'hardwareAccelerationModeEnabledPref_',
      // <if expr="is_win">
      'isolation_state.enabled': 'isolationStateEnabledPref_',
      // </if>
    });

    // <if expr="is_win">
    PrefService.getInstance().whenInitialized().then(() => {
      this.processIsolationEnabledAtStartup_ =
          PrefService.getInstance()
              .getPref<boolean>('isolation_state.enabled')
              .value;
    });
    // </if>
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('proxyPref_') ||
        changedPrivateProperties.has('proxyOverrideRulesPref_')) {
      this.observeProxyPrefChanged_();
    }
  }

  private observeProxyPrefChanged_() {
    if (!this.proxyPref_ || !this.proxyOverrideRulesPref_) {
      return;
    }
    // TODO(dbeam): do types of policy other than USER apply on ChromeOS?
    this.isProxyEnforcedByPolicy_ = this.proxyPref_.enforcement ===
            chrome.settingsPrivate.Enforcement.ENFORCED &&
        this.proxyPref_.controlledBy ===
            chrome.settingsPrivate.ControlledBy.USER_POLICY;
    this.isProxyDefault_ =
        !this.isProxyEnforcedByPolicy_ && !this.proxyPref_.extensionId;

    // Don't need to consider multiple source display when
    // `ProxyOverrideRules` preference is not set
    if (!this.proxyOverrideRulesPref_.value ||
        this.proxyOverrideRulesPref_.value.length === 0) {
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
    if (this.proxyPref_.controlledBy !==
        this.proxyOverrideRulesPref_.controlledBy) {
      this.isProxyEnforcedByMultipleSources_ = true;
      return;
    }

    // When proxy settings and `ProxyOverrideRules` are both from policies, the
    // sources are considered to be the same
    if (this.proxyPref_.controlledBy ===
        chrome.settingsPrivate.ControlledBy.USER_POLICY) {
      this.isProxyEnforcedByMultipleSources_ = false;
      return;
    }

    // When proxy settings and `ProxyOverrideRules` are both from extension(s),
    // the sources are considered to be the same only if they are set by the
    // same extension
    this.isProxyEnforcedByMultipleSources_ =
        (this.proxyPref_.extensionId !==
         this.proxyOverrideRulesPref_.extensionId);
  }

  protected onDisableExtensionClick_() {
    // TODO(dbeam): this is a pretty huge bummer. It means there are things
    // (inputs) that our prefs system is not observing. And that changes from
    // other sources (i.e. disabling/enabling an extension from
    // chrome://extensions or from the omnibox directly) will not update
    // |this.getPref('proxy')| directly (nor the UI). We should fix this
    // eventually.
    this.fire('refresh-pref', 'proxy');
  }

  protected onProxyClick_() {
    if (this.isProxyDefault_) {
      SystemPageBrowserProxyImpl.getInstance().showProxySettings();
    }
  }

  protected onRestartClick_(e: Event) {
    // Prevent event from bubbling up to the toggle button.
    e.stopPropagation();
    this.performRestart(RestartType.RESTART);
  }

  /**
   * @param enabled Whether hardware acceleration is currently enabled.
   */
  protected shouldShowRestart_(): boolean {
    if (!this.hardwareAccelerationModeEnabledPref_) {
      return false;
    }
    const proxy = SystemPageBrowserProxyImpl.getInstance();
    return this.hardwareAccelerationModeEnabledPref_.value !==
        proxy.wasHardwareAccelerationEnabledAtStartup();
  }

  // <if expr="is_win">
  protected shouldShowIsolationRestart_(): boolean {
    if (this.processIsolationEnabledAtStartup_ === undefined ||
        !this.isolationStateEnabledPref_) {
      return false;
    }
    return this.isolationStateEnabledPref_.value !==
        this.processIsolationEnabledAtStartup_;
  }
  // </if>

  // <if expr="_google_chrome">
  protected onOnDeviceAiLinkClick_(): void {
    Router.getInstance().navigateTo(routes.AI);
  }
  // </if>

  // <if expr="_google_chrome and is_win">
  protected onFeatureNotificationsSettingsBooleanControlChange_(e: Event) {
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
