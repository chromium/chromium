// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-category-default-radio-group' is the polymer element for showing
 * a certain category under Site Settings.
 */
import '../controls/collapse_radio_button.js';
import '../controls/settings_radio_group.js';

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsCollapseRadioButtonElement} from '../controls/collapse_radio_button.js';
import type {SettingsRadioGroupElement} from '../controls/settings_radio_group.js';
import {loadTimeData} from '../i18n_setup.js';

import {ContentSetting, ContentSettingsTypes} from './constants.js';
import {getCss} from './settings_category_default_radio_group.css.js';
import {getHtml} from './settings_category_default_radio_group.html.js';
import type {DefaultContentSetting} from './site_settings_browser_proxy.js';
import {DefaultSettingSource} from './site_settings_browser_proxy.js';
import {SiteSettingsMixinLit} from './site_settings_mixin_lit.js';

export interface SettingsCategoryDefaultRadioGroupElement {
  $: {
    allowRadioOption: SettingsCollapseRadioButtonElement,
    askRadioOption: SettingsCollapseRadioButtonElement,
    blockRadioOption: SettingsCollapseRadioButtonElement,
    settingsCategoryDefaultRadioGroup: SettingsRadioGroupElement,
  };
}

const SettingsCategoryDefaultRadioGroupElementBase =
    SiteSettingsMixinLit(WebUiListenerMixinLit(CrLitElement));

export class SettingsCategoryDefaultRadioGroupElement extends
    SettingsCategoryDefaultRadioGroupElementBase {
  static get is() {
    return 'settings-category-default-radio-group';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      header: {type: String},
      description: {type: String},

      // The default values here must be explicitly specified. The reason is
      // that even if the HTML for a specific category type does not supply,
      // say, an `allowOptionLabel`, the property cannot remain `undefined`, but
      // must be set to `null`, which causes the the computed property assigned
      // to the radio button's `hidden` attribute to be calculated.
      allowOptionLabel: {type: String},
      allowOptionSubLabel: {type: String},
      allowOptionIcon: {type: String},

      askOptionLabel: {type: String},
      askOptionSubLabel: {type: String},
      askOptionIcon: {type: String},

      blockOptionLabel: {type: String},
      blockOptionSubLabel: {type: String},
      blockOptionIcon: {type: String},

      selectedValue: {
        type: String,
        notify: true,
      },

      /**
       * Preference object used to keep track of the selected content setting
       * option.
       */
      pref_: {type: Object},
    };
  }

  accessor header: string =
      loadTimeData.getString('siteSettingsDefaultBehavior');
  accessor description: string =
      loadTimeData.getString('siteSettingsDefaultBehaviorDescription');
  accessor allowOptionLabel: string = '';
  accessor allowOptionSubLabel: string = '';
  accessor allowOptionIcon: string = '';
  accessor askOptionLabel: string = '';
  accessor askOptionSubLabel: string = '';
  accessor askOptionIcon: string = '';
  accessor blockOptionLabel: string = '';
  accessor blockOptionSubLabel: string = '';
  accessor blockOptionIcon: string = '';
  accessor selectedValue: string = '';
  protected accessor pref_:
      chrome.settingsPrivate.PrefObject<ContentSetting> = {
    key: '',
    type: chrome.settingsPrivate.PrefType.STRING,
    value: ContentSetting.DEFAULT,
  };

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'contentSettingCategoryChanged',
        (category: ContentSettingsTypes) => this.onCategoryChanged_(category));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('category')) {
      this.onCategoryChanged_(this.category);
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('pref_')) {
      this.selectedValue = this.pref_.value;
    }
  }

  protected getButtonClass_(subLabel: string): string {
    return subLabel ? 'two-line' : '';
  }

  protected onSelectedRadioChange_() {
    const radioGroup = this.shadowRoot.querySelector('settings-radio-group');
    assert(radioGroup);
    assert(
        this.pref_.enforcement !== chrome.settingsPrivate.Enforcement.ENFORCED);
    const value =
        (radioGroup.pref?.value || radioGroup.selected) as ContentSetting;
    this.pref_ = {...this.pref_, value};
    assert(this.category);
    this.browserProxy.setDefaultValueForContentType(this.category, value);
  }

  /**
   * Update the pref values from the content settings.
   * @param update The updated content setting value.
   */
  private updatePref_(update: DefaultContentSetting) {
    const pref: chrome.settingsPrivate.PrefObject<ContentSetting> = {
      key: '',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: update.setting,
    };

    if (update.source !== undefined &&
        update.source !== DefaultSettingSource.PREFERENCE) {
      pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      let controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
      switch (update.source) {
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

  private async onCategoryChanged_(category: ContentSettingsTypes|undefined) {
    if (this.category === undefined || category !== this.category) {
      return;
    }
    const defaultValue =
        await this.browserProxy.getDefaultValueForContentType(this.category);
    this.updatePref_(defaultValue);
  }

  /**
   * Check if the category is popups and the user is logged in guest mode.
   * Users in guest mode are not allowed to modify pop-ups content setting.
   */
  protected isRadioGroupDisabled_(): boolean {
    return this.category === ContentSettingsTypes.POPUPS &&
        loadTimeData.getBoolean('isGuest');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-category-default-radio-group':
        SettingsCategoryDefaultRadioGroupElement;
  }
}

customElements.define(
    SettingsCategoryDefaultRadioGroupElement.is,
    SettingsCategoryDefaultRadioGroupElement);
