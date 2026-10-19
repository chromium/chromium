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

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsCollapseRadioButtonElement} from '../controls/collapse_radio_button.js';
import type {SettingsRadioGroupElement} from '../controls/settings_radio_group.js';
import {loadTimeData} from '../i18n_setup.js';

import {ContentSettingsTypes} from './constants.js';
import type {ContentSetting} from './constants.js';
import {getCss} from './settings_category_default_radio_group.css.js';
import {getHtml} from './settings_category_default_radio_group.html.js';
import {DefaultSettingSource} from './site_settings_browser_proxy.js';
import type {DefaultContentSetting} from './site_settings_browser_proxy.js';
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
    PrefServiceObserverMixinLit(
        SiteSettingsMixinLit(WebUiListenerMixinLit(CrLitElement)));

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
       * Key of the synthetic preference used to keep track of the selected
       * content setting option.
       */
      prefKey_: {type: String},
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
  protected accessor prefKey_: string = '';

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'contentSettingCategoryChanged',
        async (category: ContentSettingsTypes) => {
          if (category === this.category) {
            const defaultValue =
                await this.browserProxy.getDefaultValueForContentType(
                    this.category);
            this.updatePref_(defaultValue);
          }
        });
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('category') && this.category !== undefined) {
      assert(!changedProperties.get('category'));
      this.initializePref_();
    }
  }

  private async initializePref_() {
    assert(this.category);
    this.prefKey_ = `synthetic.settings.category_default.${this.category}`;
    this.addPrefObserver<ContentSetting>(this.prefKey_, pref => {
      this.selectedValue = pref.value;
    });
    const [defaultValue] = await Promise.all([
      this.browserProxy.getDefaultValueForContentType(this.category),
      PrefService.getInstance().whenInitialized(),
    ]);
    this.updatePref_(defaultValue);
  }

  protected getButtonClass_(subLabel: string): string {
    return subLabel ? 'two-line' : '';
  }

  protected onSelectedRadioChange_() {
    assert(this.prefKey_);
    const pref =
        PrefService.getInstance().getPref<ContentSetting>(this.prefKey_);
    assert(pref.enforcement !== chrome.settingsPrivate.Enforcement.ENFORCED);
    assert(this.category);
    this.browserProxy.setDefaultValueForContentType(this.category, pref.value);
  }

  /**
   * Update the pref values from the content settings.
   */
  private updatePref_(update: DefaultContentSetting) {
    if (!this.isConnected) {
      return;
    }

    const pref: chrome.settingsPrivate.PrefObject<ContentSetting> = {
      key: this.prefKey_,
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

    PrefService.getInstance().setSyntheticPref(pref);
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
