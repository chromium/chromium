// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-on-startup-page' is a settings page.
 */
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/controlled_radio_button.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import '../controls/settings_radio_group.js';
import './startup_urls_page.js';
import '../i18n_setup.js';
import '../settings_shared.css.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {NtpExtension} from './on_startup_browser_proxy.js';
import {OnStartupBrowserProxyImpl} from './on_startup_browser_proxy.js';
import {getTemplate} from './on_startup_page.html.js';


/** Enum values for the 'session.restore_on_startup' preference. */
enum PrefValues {
  CONTINUE = 1,
  OPEN_NEW_TAB = 5,
  OPEN_SPECIFIC = 4,
  CONTINUE_AND_OPEN_SPECIFIC = 6,
}

const SettingsOnStartupPageElementBase = WebUiListenerMixin(PolymerElement);

export class SettingsOnStartupPageElement extends
    SettingsOnStartupPageElementBase {
  static get is() {
    return 'settings-on-startup-page';
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

      ntpExtension_: Object,

      prefValues_: {readOnly: true, type: Object, value: PrefValues},
    };
  }

  prefs: Object;
  private ntpExtension_: NtpExtension|null;

  override connectedCallback() {
    super.connectedCallback();

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
   * @param restoreOnStartup Enum value from PrefValues.
   * @return Whether the "open specific pages" or "continue and open specific
   *     pages" is selected.
   */
  private showStartupUrls_(restoreOnStartup: PrefValues): boolean {
    return restoreOnStartup === PrefValues.OPEN_SPECIFIC ||
        restoreOnStartup === PrefValues.CONTINUE_AND_OPEN_SPECIFIC;
  }

  /**
   * Determine whether to show "continue and open specific pages" option.
   * @param restoreOnStartup pref.
   * @return Whether the restoreOnStartup pref is recommended or enforced by
   *     policy.
   */
  private showContinueAndOpenSpecific_(pref: chrome.settingsPrivate.PrefObject):
      boolean {
    return pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED ||
        pref.enforcement === chrome.settingsPrivate.Enforcement.RECOMMENDED;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-on-startup-page': SettingsOnStartupPageElement;
  }
}

customElements.define(
    SettingsOnStartupPageElement.is, SettingsOnStartupPageElement);
