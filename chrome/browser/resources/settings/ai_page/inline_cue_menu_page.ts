// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '../icons.html.js';
import '../site_settings/add_site_dialog.js';
import '../site_favicon.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_subpage.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {SettingsViewMixinLit} from '../settings_page/settings_view_mixin_lit.js';
import {ContentSetting, ContentSettingsTypes} from '../site_settings/constants.js';
import type {RawSiteException, SiteSettingsBrowserProxy} from '../site_settings/site_settings_browser_proxy.js';
import {DefaultSettingSource, SiteSettingsBrowserProxyImpl} from '../site_settings/site_settings_browser_proxy.js';
import {isSettingEnabled} from '../site_settings/site_settings_util.js';

import {getCss} from './inline_cue_menu_page.css.js';
import {getHtml} from './inline_cue_menu_page.html.js';

export interface InlineCueMenuPageElement {
  $: {
    mainToggle: SettingsToggleButtonElement,
  };
}

const InlineCueMenuPageElementBase =
    SettingsViewMixinLit(WebUiListenerMixinLit(I18nMixinLit(CrLitElement)));

export class InlineCueMenuPageElement extends InlineCueMenuPageElementBase {
  static get is() {
    return 'settings-inline-cue-menu-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      pref_: {type: Object},
      sites_: {type: Array},
      showAddSiteDialog_: {type: Boolean},
    };
  }

  protected accessor pref_: chrome.settingsPrivate.PrefObject<boolean> = {
    key: '',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: false,
  };
  protected accessor sites_: RawSiteException[] = [];
  protected accessor showAddSiteDialog_: boolean = false;
  private browserProxy_: SiteSettingsBrowserProxy =
      SiteSettingsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'contentSettingCategoryChanged',
        (category: ContentSettingsTypes) => this.onCategoryChanged_(category));

    this.addWebUiListener(
        'contentSettingSitePermissionChanged',
        (category: ContentSettingsTypes) => {
          if (category === ContentSettingsTypes.INLINE_CUE_MENU) {
            this.updateSites_();
          }
        });

    this.updateToggleValue_();
    this.updateSites_();
  }

  private onCategoryChanged_(category: ContentSettingsTypes) {
    if (category !== ContentSettingsTypes.INLINE_CUE_MENU) {
      return;
    }

    this.updateToggleValue_();
    this.updateSites_();
  }

  private async updateSites_() {
    const sites = await this.browserProxy_.getExceptionList(
        ContentSettingsTypes.INLINE_CUE_MENU);
    this.sites_ = sites.filter(site => site.setting === ContentSetting.BLOCK);
  }

  protected hasSites_(): boolean {
    return this.sites_.length > 0;
  }

  protected onAddSiteClick_() {
    chrome.metricsPrivate.recordUserAction(
        'Settings.AiPage.InlineCueMenu.AddSiteClicked');
    this.showAddSiteDialog_ = true;
  }

  protected onAddSiteDialogClose_() {
    this.showAddSiteDialog_ = false;
    this.updateSites_();
  }

  protected onDeleteSiteClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    const site = this.sites_[index];
    this.browserProxy_.resetCategoryPermissionForPattern(
        site.origin, site.embeddingOrigin, ContentSettingsTypes.INLINE_CUE_MENU,
        site.incognito);
  }

  private async updateToggleValue_() {
    const defaultValue = await this.browserProxy_.getDefaultValueForContentType(
        ContentSettingsTypes.INLINE_CUE_MENU);

    const pref: chrome.settingsPrivate.PrefObject<boolean> = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: isSettingEnabled(defaultValue.setting),
    };

    // Update pref_ policy enforcement properties to match standard
    // ContentSettings category controls (see
    // settings_category_default_radio_group.ts).
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

  protected onMainToggleSettingsBooleanControlChange_() {
    if (this.$.mainToggle.checked) {
      chrome.metricsPrivate.recordUserAction(
          'Settings.AiPage.InlineCueMenu.Enabled');
    } else {
      chrome.metricsPrivate.recordUserAction(
          'Settings.AiPage.InlineCueMenu.Disabled');
    }
    this.browserProxy_.setDefaultValueForContentType(
        ContentSettingsTypes.INLINE_CUE_MENU,
        this.$.mainToggle.checked ? ContentSetting.ALLOW :
                                    ContentSetting.BLOCK);
  }

  protected getPreviewText_(): TrustedHTML {
    return this.i18nAdvanced('siteSettingsInlineCueMenuPreviewText', {
      tags: ['span'],
      attrs: ['class'],
    });
  }

  // SettingsViewMixinLit implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-inline-cue-menu-page': InlineCueMenuPageElement;
  }
}

customElements.define(InlineCueMenuPageElement.is, InlineCueMenuPageElement);
