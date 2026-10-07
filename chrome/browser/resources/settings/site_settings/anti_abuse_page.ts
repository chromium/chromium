// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-anti-abuse-page' is the settings page containing anti-abuse
 * settings.
 */

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_subpage.js';

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {getCss as getSettingsColumnedSectionCss} from '../settings_columned_section_lit.css.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';
import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';
import {ContentSetting, ContentSettingsTypes} from '../site_settings/constants.js';
import {DefaultSettingSource} from '../site_settings/site_settings_browser_proxy.js';
import {SiteSettingsMixinLit} from '../site_settings/site_settings_mixin_lit.js';
import {isSettingEnabled} from '../site_settings/site_settings_util.js';

import {getHtml} from './anti_abuse_page.html.js';

export interface SettingsAntiAbusePageElement {
  $: {
    toggleButton: SettingsToggleButtonElement,
  };
}

export type AntiAbusePageElement = SettingsAntiAbusePageElement;

const AntiAbuseElementBase = SettingsViewMixinLit(
    SiteSettingsMixinLit(WebUiListenerMixinLit(CrLitElement)));

export class SettingsAntiAbusePageElement extends AntiAbuseElementBase {
  static get is() {
    return 'settings-anti-abuse-page';
  }

  static override get styles() {
    return [
      getSettingsSharedCss(),
      getSettingsColumnedSectionCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Preference object used to keep track of the selected content setting
       * option.
       */
      pref_: {type: Object},
    };
  }

  protected accessor pref_: chrome.settingsPrivate.PrefObject<boolean> = {
    key: '',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: false,
  };

  protected getToggleDisabled_(): boolean {
    return this.pref_.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED;
  }

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'contentSettingCategoryChanged',
        (category: ContentSettingsTypes) => this.onCategoryChanged_(category));

    this.updateToggleValue_();
  }

  private onCategoryChanged_(category: ContentSettingsTypes) {
    if (category !== ContentSettingsTypes.ANTI_ABUSE) {
      return;
    }

    this.updateToggleValue_();
  }

  private async updateToggleValue_() {
    const defaultValue = await this.browserProxy.getDefaultValueForContentType(
        ContentSettingsTypes.ANTI_ABUSE);

    const pref: chrome.settingsPrivate.PrefObject<boolean> = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: isSettingEnabled(defaultValue.setting),
    };

    if (defaultValue.source !== undefined &&
        defaultValue.source !== DefaultSettingSource.PREFERENCE) {
      pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      let controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
      switch (defaultValue.source) {
        case DefaultSettingSource.POLICY:
          controlledBy = chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;
          break;
        case DefaultSettingSource.SUPERVISED_USER:
          controlledBy = chrome.settingsPrivate.ControlledBy.PARENT;
          break;
        case DefaultSettingSource.EXTENSION:
          controlledBy = chrome.settingsPrivate.ControlledBy.EXTENSION;
          break;
        default:
          break;
      }
      pref.controlledBy = controlledBy;
    }

    this.pref_ = pref;
  }

  /**
   * A handler for changing the default permission value for the anti-abuse
   * content type.
   */
  protected onSettingsBooleanControlChange_() {
    this.browserProxy.setDefaultValueForContentType(
        ContentSettingsTypes.ANTI_ABUSE,
        this.$.toggleButton.checked ? ContentSetting.ALLOW :
                                      ContentSetting.BLOCK);
  }

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-anti-abuse-page': SettingsAntiAbusePageElement;
  }
}

customElements.define(
    SettingsAntiAbusePageElement.is, SettingsAntiAbusePageElement);
