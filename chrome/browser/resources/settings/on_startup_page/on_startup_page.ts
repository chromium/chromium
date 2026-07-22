// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-on-startup-page' is a settings page.
 */
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../controls/controlled_radio_button.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import '../controls/settings_radio_group.js';
import '../i18n_setup.js';
import '../settings_page/settings_section.js';
import '../settings_shared.css.js';
import './startup_urls_page.js';
// <if expr="is_win">
import '../controls/settings_toggle_button.js';

// </if>

import {PrefServiceObserverMixin} from '/shared/settings/prefs2/pref_service_observer_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
// <if expr="is_win">
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
// </if>
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getSearchManager} from '../search_settings.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';

import type {NtpExtension} from './on_startup_browser_proxy.js';
import {OnStartupBrowserProxyImpl} from './on_startup_browser_proxy.js';
import {getTemplate} from './on_startup_page.html.js';


/** Enum values for the 'session.restore_on_startup' preference. */
export enum PrefValues {
  CONTINUE = 1,
  OPEN_NEW_TAB = 5,
  OPEN_SPECIFIC = 4,
  CONTINUE_AND_OPEN_SPECIFIC = 6,
}

const SettingsOnStartupPageElementBase =
    PrefServiceObserverMixin(WebUiListenerMixin(PolymerElement));

export class SettingsOnStartupPageElement extends
    SettingsOnStartupPageElementBase implements SettingsPlugin {
  static get is() {
    return 'settings-on-startup-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      restoreOnStartupPref_: Object,

      // <if expr="is_win">
      isForegroundLaunchFeatureEnabled_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('isForegroundLaunchFeatureEnabled'),
      },
      // </if>

      ntpExtension_: Object,

      prefValuesEnum_: {readOnly: true, type: Object, value: PrefValues},
    };
  }

  declare private restoreOnStartupPref_:
      chrome.settingsPrivate.PrefObject<PrefValues>|undefined;
  // <if expr="is_win">
  declare private isForegroundLaunchFeatureEnabled_: boolean;
  // </if>
  declare private ntpExtension_: NtpExtension|null;

  override connectedCallback() {
    super.connectedCallback();

    this.mirrorPref('session.restore_on_startup', 'restoreOnStartupPref_');

    const updateNtpExtension = (ntpExtension: NtpExtension|null) => {
      // Note that |ntpExtension| is empty if there is no NTP extension.
      this.ntpExtension_ = ntpExtension;
    };
    OnStartupBrowserProxyImpl.getInstance().getNtpExtension().then(
        updateNtpExtension);
    this.addWebUiListener('update-ntp-extension', updateNtpExtension);
  }

  private getName_(value: number): string {
    return value.toString();
  }

  /**
   * Determine whether to show the user defined startup pages.
   * @return Whether the "open specific pages" or "continue and open specific
   *     pages" is selected.
   */
  private showStartupUrls_(): boolean {
    assert(this.restoreOnStartupPref_);
    const value = this.restoreOnStartupPref_.value;
    return value === PrefValues.OPEN_SPECIFIC ||
        value === PrefValues.CONTINUE_AND_OPEN_SPECIFIC;
  }

  /**
   * Determine whether to show "continue and open specific pages" option.
   * @return Whether the restoreOnStartup pref is recommended or enforced by
   *     policy.
   */
  private showContinueAndOpenSpecific_(): boolean {
    assert(this.restoreOnStartupPref_);
    return this.restoreOnStartupPref_.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED ||
        this.restoreOnStartupPref_.enforcement ===
        chrome.settingsPrivate.Enforcement.RECOMMENDED;
  }

  // SettingsPlugin implementation
  async searchContents(query: string) {
    const searchRequest = await getSearchManager().search(query, this);
    return searchRequest.getSearchResult();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-on-startup-page': SettingsOnStartupPageElement;
  }
}

customElements.define(
    SettingsOnStartupPageElement.is, SettingsOnStartupPageElement);
