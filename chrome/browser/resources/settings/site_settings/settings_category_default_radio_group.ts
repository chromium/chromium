// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-category-default-radio-group' is the polymer element for showing
 * a certain category under Site Settings.
 */
import '../settings_shared.css.js';
import '../controls/collapse_radio_button.js';
import '../controls/settings_radio_group.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsCollapseRadioButtonElement} from '../controls/collapse_radio_button.js';
import type {SettingsRadioGroupElement} from '../controls/settings_radio_group.js';
import {loadTimeData} from '../i18n_setup.js';

import {ContentSetting, ContentSettingsTypes} from './constants.js';
import {getTemplate} from './settings_category_default_radio_group.html.js';
import type {DefaultContentSetting} from './site_settings_browser_proxy.js';
import {DefaultSettingSource} from './site_settings_browser_proxy.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';

export interface SettingsCategoryDefaultRadioGroupElement {
  $: {
    allowRadioOption: SettingsCollapseRadioButtonElement,
    askRadioOption: SettingsCollapseRadioButtonElement,
    blockRadioOption: SettingsCollapseRadioButtonElement,
    settingsCategoryDefaultRadioGroup: SettingsRadioGroupElement,
  };
}

const SettingsCategoryDefaultRadioGroupElementBase =
    SiteSettingsMixin(WebUiListenerMixin(PolymerElement));

export class SettingsCategoryDefaultRadioGroupElement extends
    SettingsCategoryDefaultRadioGroupElementBase {
  static get is() {
    return 'settings-category-default-radio-group';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      header: {
        type: String,
        value() {
          return loadTimeData.getString('siteSettingsDefaultBehavior');
        },
      },

      description: {
        type: String,
        value() {
          return loadTimeData.getString(
              'siteSettingsDefaultBehaviorDescription');
        },
      },

      // The default values here must be explicitly specified. The reason is
      // that even if the HTML for a specific category type does not supply,
      // say, an `allowOptionLabel`, the property cannot remain `undefined`, but
      // must be set to `null`, which causes the the computed property assigned
      // to the radio button's `hidden` attribute to be calculated.
      allowOptionLabel: {type: String, value: null},
      allowOptionSubLabel: String,
      allowOptionIcon: String,

      askOptionLabel: {type: String, value: null},
      askOptionSubLabel: String,
      askOptionIcon: String,

      blockOptionLabel: {type: String, value: null},
      blockOptionSubLabel: String,
      blockOptionIcon: String,

      selectedValue: {
        type: String,
        computed: 'getSelectedValue_(pref_.value)',
        readOnly: true,
        notify: true,
      },

      contentSettingEnum_: {
        type: Object,
        value: ContentSetting,
      },

      /**
       * Preference object used to keep track of the selected content setting
       * option.
       */
      pref_: {
        type: Object,
        value() {
          return {
            type: chrome.settingsPrivate.PrefType.STRING,
            value: '',  // No element is selected until the value is loaded.
          };
        },
      },
    };
  }

  static get observers() {
    return [
      'onCategoryChanged_(category)',
    ];
  }

  declare header: string;
  declare description: string;
  declare allowOptionLabel: string;
  declare allowOptionSubLabel: string;
  declare allowOptionIcon: string;
  declare askOptionLabel: string;
  declare askOptionSubLabel: string;
  declare askOptionIcon: string;
  declare blockOptionLabel: string;
  declare blockOptionSubLabel: string;
  declare blockOptionIcon: string;
  declare selectedValue: string;

  declare private pref_: chrome.settingsPrivate.PrefObject<ContentSetting>;

  override ready() {
    super.ready();

    this.addWebUiListener(
        'contentSettingCategoryChanged',
        (category: ContentSettingsTypes) => this.onCategoryChanged_(category));
  }

  private getButtonClass_(subLabel: string): string {
    return subLabel ? 'two-line' : '';
  }

  private getSelectedValue_(value: string): string {
    return value;
  }

  /**
   * A handler for when the user selects a differenet option in the nested
   * radio group.
   */
  private onSelectedRadioChanged_() {
    assert(
        this.pref_.enforcement !== chrome.settingsPrivate.Enforcement.ENFORCED);
    this.browserProxy.setDefaultValueForContentType(
        this.category, this.pref_.value);
  }

  /**
   * Update the pref values from the content settings.
   * @param update The updated content setting value.
   */
  private updatePref_(update: DefaultContentSetting) {
    if (update.source !== undefined &&
        update.source !== DefaultSettingSource.PREFERENCE) {
      this.set(
          'pref_.enforcement', chrome.settingsPrivate.Enforcement.ENFORCED);
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
      this.set('pref_.controlledBy', controlledBy);
    } else {
      this.set('pref_.enforcement', undefined);
      this.set('pref_.controlledBy', undefined);
    }

    this.set('pref_.value', update.setting);
  }

  private async onCategoryChanged_(category: ContentSettingsTypes) {
    if (category !== this.category) {
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
  private isRadioGroupDisabled_(): boolean {
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
