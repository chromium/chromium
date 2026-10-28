// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../controls/settings_toggle_button.js';
import '../settings_page/settings_subpage.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';
import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';

import type {OnDeviceAiBrowserProxy, OnDeviceAiEnabled} from './on_device_ai_browser_proxy.js';
import {OnDeviceAiBrowserProxyImpl} from './on_device_ai_browser_proxy.js';
import {getHtml} from './on_device_ai_page.html.js';

const ON_DEVICE_AI_PREF_KEY = 'synthetic.settings.on_device_ai_enabled';

export interface SettingsOnDeviceAiPageElement {
  $: {
    onDeviceAiToggle: SettingsToggleButtonElement,
  };
}

const SettingsOnDeviceAiPageElementBase =
    WebUiListenerMixinLit(SettingsViewMixinLit(CrLitElement));

export class SettingsOnDeviceAiPageElement extends
    SettingsOnDeviceAiPageElementBase {
  static get is() {
    return 'settings-on-device-ai-page';
  }

  static override get styles() {
    return getSettingsSharedCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  private onDeviceAiBrowserProxy_: OnDeviceAiBrowserProxy =
      OnDeviceAiBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    const setOnDeviceAiPref = (onDeviceAiEnabled: OnDeviceAiEnabled) =>
        this.setOnDeviceAiPref_(onDeviceAiEnabled);
    this.addWebUiListener('on-device-ai-enabled-changed', setOnDeviceAiPref);
    Promise
        .all([
          this.onDeviceAiBrowserProxy_.getOnDeviceAiEnabled(),
          PrefService.getInstance().whenInitialized(),
        ])
        .then(([onDeviceAiEnabled]) => setOnDeviceAiPref(onDeviceAiEnabled));
  }

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }

  protected onOnDeviceAiSubLabelLinkClicked_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('onDeviceAiLearnMoreUrl'));
  }

  protected onOnDeviceAiSettingsBooleanControlChange_(e: Event) {
    const enabled = (e.target as SettingsToggleButtonElement).checked;
    this.onDeviceAiBrowserProxy_.setOnDeviceAiEnabled(enabled);
  }

  private setOnDeviceAiPref_(onDeviceAiEnabled: OnDeviceAiEnabled) {
    const pref: chrome.settingsPrivate.PrefObject<boolean> = {
      key: ON_DEVICE_AI_PREF_KEY,
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: onDeviceAiEnabled.enabled,
    };

    if (!onDeviceAiEnabled.allowedByPolicy) {
      pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      pref.controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
      pref.value = false;
    }

    PrefService.getInstance().setSyntheticPref(pref);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-on-device-ai-page': SettingsOnDeviceAiPageElement;
  }
}

customElements.define(
    SettingsOnDeviceAiPageElement.is, SettingsOnDeviceAiPageElement);
