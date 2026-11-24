// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './category_setting_exceptions.js';
import './settings_category_default_radio_group.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_subpage.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';
import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';

import type {SiteSettingsBrowserProxy} from './site_settings_browser_proxy.js';
import {SiteSettingsBrowserProxyImpl} from './site_settings_browser_proxy.js';
import {getCss as getSiteSettingsSharedCss} from './site_settings_shared_lit.css.js';
import {getHtml} from './sound_page.html.js';

interface BlockAutoplayStatus {
  enabled: boolean;
  pref: chrome.settingsPrivate.PrefObject<boolean>;
}

const BLOCK_AUTOPLAY_PREF_KEY =
    'synthetic.settings.site_settings.block_autoplay';

export interface SoundPageElement {
  $: {
    blockAutoplaySetting: SettingsToggleButtonElement,
  };
}

const SoundPageElementBase =
    SettingsViewMixinLit(WebUiListenerMixinLit(CrLitElement));

export class SoundPageElement extends SoundPageElementBase {
  static get is() {
    return 'settings-sound-page';
  }

  static override get styles() {
    return [
      getSettingsSharedCss(),
      getSiteSettingsSharedCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      searchTerm_: {type: String},
      blockAutoplayEnabled_: {type: Boolean},
    };
  }

  protected accessor searchTerm_: string = '';
  protected accessor blockAutoplayEnabled_: boolean = false;

  protected readonly enableBlockAutoplayContentSetting_: boolean =
      loadTimeData.getBoolean('enableBlockAutoplayContentSetting');
  private browserProxy_: SiteSettingsBrowserProxy =
      SiteSettingsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'onBlockAutoplayStatusChanged',
        (status: BlockAutoplayStatus) =>
            this.onBlockAutoplayStatusChanged_(status));
  }

  protected onSearchTermChanged_(e: CustomEvent<{value: string}>) {
    this.searchTerm_ = e.detail.value;
  }

  // Called when the block autoplay status changes.
  private onBlockAutoplayStatusChanged_(autoplayStatus: BlockAutoplayStatus) {
    this.blockAutoplayEnabled_ = autoplayStatus.enabled;
    PrefService.getInstance().setSyntheticPref({
      ...autoplayStatus.pref,
      key: BLOCK_AUTOPLAY_PREF_KEY,
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
    });
  }

  // Updates the block autoplay pref when the toggle is changed.
  protected onSettingsBooleanControlChange_(event: Event) {
    const target = event.target as SettingsToggleButtonElement;
    this.browserProxy_.setBlockAutoplayEnabled(target.checked);
  }

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-sound-page': SoundPageElement;
  }
}

customElements.define(SoundPageElement.is, SoundPageElement);
