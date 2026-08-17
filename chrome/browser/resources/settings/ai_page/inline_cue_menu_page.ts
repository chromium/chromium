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
import '../site_settings/site_settings_shared.css.js';
import '../controls/settings_toggle_button.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';
import {ContentSetting, ContentSettingsTypes} from '../site_settings/constants.js';
import type {RawSiteException} from '../site_settings/site_settings_browser_proxy.js';
import {DefaultSettingSource} from '../site_settings/site_settings_browser_proxy.js';
import {SiteSettingsMixin} from '../site_settings/site_settings_mixin.js';
import {isSettingEnabled} from '../site_settings/site_settings_util.js';

import {getTemplate} from './inline_cue_menu_page.html.js';

export interface InlineCueMenuPageElement {
  $: {
    mainToggle: SettingsToggleButtonElement,
  };
}

const InlineCueMenuPageElementBase = SettingsViewMixin(SiteSettingsMixin(
    PrefsMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))));

export class InlineCueMenuPageElement extends InlineCueMenuPageElementBase {
  static get is() {
    return 'settings-inline-cue-menu-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      pref_: {
        type: Object,
        value() {
          return {type: chrome.settingsPrivate.PrefType.BOOLEAN};
        },
      },

      sites_: {
        type: Array,
        value: () => [],
      },

      showAddSiteDialog_: {
        type: Boolean,
        value: false,
      },

      hasIncognito_: {
        type: Boolean,
        value: false,
      },

      // Expose ContentSettingsTypes enum to the HTML template.
      contentSettingsTypesEnum_: {
        type: Object,
        value: ContentSettingsTypes,
      },

      // Expose ContentSetting enum to the HTML template.
      contentSettingEnum_: {
        type: Object,
        value: ContentSetting,
      },
    };
  }

  declare private pref_: chrome.settingsPrivate.PrefObject<boolean>;
  declare private sites_: RawSiteException[];
  declare private showAddSiteDialog_: boolean;
  declare private hasIncognito_: boolean;

  override ready() {
    super.ready();

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
    const sites = await this.browserProxy.getExceptionList(
        ContentSettingsTypes.INLINE_CUE_MENU);
    this.sites_ = sites.filter(site => site.setting === ContentSetting.BLOCK);
  }

  private hasSites_(): boolean {
    return this.sites_.length > 0;
  }

  private onAddSiteClick_() {
    chrome.metricsPrivate.recordUserAction(
        'Settings.AiPage.InlineCueMenu.AddSiteClicked');
    this.showAddSiteDialog_ = true;
  }

  private onAddSiteDialogClosed_() {
    this.showAddSiteDialog_ = false;
    this.updateSites_();
  }

  private onDeleteSiteClick_(e: DomRepeatEvent<RawSiteException>) {
    const site = e.model.item;
    this.browserProxy.resetCategoryPermissionForPattern(
        site.origin, site.embeddingOrigin, ContentSettingsTypes.INLINE_CUE_MENU,
        site.incognito);
  }

  private async updateToggleValue_() {
    const defaultValue = await this.browserProxy.getDefaultValueForContentType(
        ContentSettingsTypes.INLINE_CUE_MENU);

    // Update pref_ policy enforcement properties to match standard
    // ContentSettings category controls (see
    // settings_category_default_radio_group.ts).
    if (defaultValue.source !== undefined &&
        defaultValue.source !== DefaultSettingSource.PREFERENCE) {
      this.set(
          'pref_.enforcement', chrome.settingsPrivate.Enforcement.ENFORCED);
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
      this.set('pref_.controlledBy', controlledBy);
    } else {
      this.set('pref_.enforcement', null);
      this.set('pref_.controlledBy', null);
    }

    this.set('pref_.value', isSettingEnabled(defaultValue.setting));
  }

  private onMainToggleChange_() {
    if (this.$.mainToggle.checked) {
      chrome.metricsPrivate.recordUserAction(
          'Settings.AiPage.InlineCueMenu.Enabled');
    } else {
      chrome.metricsPrivate.recordUserAction(
          'Settings.AiPage.InlineCueMenu.Disabled');
    }
    this.browserProxy.setDefaultValueForContentType(
        ContentSettingsTypes.INLINE_CUE_MENU,
        this.$.mainToggle.checked ? ContentSetting.ALLOW :
                                    ContentSetting.BLOCK);
  }

  private getPreviewText_(): TrustedHTML {
    return this.i18nAdvanced('siteSettingsInlineCueMenuPreviewText', {
      tags: ['span'],
      attrs: ['class'],
    });
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-inline-cue-menu-page': InlineCueMenuPageElement;
  }
}

customElements.define(InlineCueMenuPageElement.is, InlineCueMenuPageElement);
