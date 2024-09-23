// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-category-default-radio-group' is the polymer element for showing
 * a certain category under Site Settings.
 */
import '../settings_shared.css.js';
import '../controls/settings_radio_group.js';
import '../privacy_page/collapse_radio_button.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsRadioGroupElement} from '../controls/settings_radio_group.js';
import {loadTimeData} from '../i18n_setup.js';
import type {SettingsCollapseRadioButtonElement} from '../privacy_page/collapse_radio_button.js';

import {ContentSetting, ContentSettingsTypes} from './constants.js';
import {getTemplate} from './settings_category_default_radio_group.html.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';
import type {DefaultContentSetting} from './site_settings_prefs_browser_proxy.js';
import {DefaultSettingSource} from './site_settings_prefs_browser_proxy.js';

/**
 * Selected content setting radio option.
 */
export enum SiteContentRadioSetting {
  DISABLED = 0,
  ENABLED = 1,
}

export interface SettingsCategoryDefaultRadioGroupElement {
  $: {
    enabledRadioOption: SettingsCollapseRadioButtonElement,
    disabledRadioOption: SettingsCollapseRadioButtonElement,
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

      allowOptionLabel: String,
      allowOptionSubLabel: String,
      allowOptionIcon: String,

      blockOptionLabel: String,
      blockOptionSubLabel: String,
      blockOptionIcon: String,

      siteContentRadioSettingEnum_: {
        type: Object,
        value: SiteContentRadioSetting,
      },

      /**
       * Preference object used to keep track of the selected content setting
       * option.
       */
      pref_: {
        type: Object,
        value() {
          return {
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: -1,  // No element is selected until the value is loaded.
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

  header: string;
  description: string;
  allowOptionLabel: string;
  allowOptionSubLabel: string;
  allowOptionIcon: string;
  blockOptionLabel: string;
  blockOptionSubLabel: string;
  blockOptionIcon: string;
  private pref_: chrome.settingsPrivate.PrefObject<number>;

  override ready() {
    super.ready();

    this.addWebUiListener(
        'contentSettingCategoryChanged',
        (category: ContentSettingsTypes) => this.onCategoryChanged_(category));
  }

  private getAllowOptionForCategory_(): ContentSetting {
    switch (this.category) {
      case ContentSettingsTypes.ADS:
      case ContentSettingsTypes.AUTOMATIC_FULLSCREEN:
      case ContentSettingsTypes.BACKGROUND_SYNC:
      case ContentSettingsTypes.FEDERATED_IDENTITY_API:
      case ContentSettingsTypes.IMAGES:
      case ContentSettingsTypes.JAVASCRIPT:
      case ContentSettingsTypes.JAVASCRIPT_OPTIMIZER:
      case ContentSettingsTypes.MIXEDSCRIPT:
      case ContentSettingsTypes.PAYMENT_HANDLER:
      case ContentSettingsTypes.POPUPS:
      case ContentSettingsTypes.PROTECTED_CONTENT:
      case ContentSettingsTypes.PROTOCOL_HANDLERS:
      case ContentSettingsTypes.SENSORS:
      case ContentSettingsTypes.SOUND:
        // "Allowed" vs "Blocked".
        return ContentSetting.ALLOW;
      case ContentSettingsTypes.AR:
      case ContentSettingsTypes.AUTO_PICTURE_IN_PICTURE:
      case ContentSettingsTypes.AUTOMATIC_DOWNLOADS:
      case ContentSettingsTypes.BLUETOOTH_DEVICES:
      case ContentSettingsTypes.BLUETOOTH_SCANNING:
      case ContentSettingsTypes.CAMERA:
      case ContentSettingsTypes.CAPTURED_SURFACE_CONTROL:
      case ContentSettingsTypes.CLIPBOARD:
      case ContentSettingsTypes.FILE_SYSTEM_WRITE:
      case ContentSettingsTypes.GEOLOCATION:
      case ContentSettingsTypes.HAND_TRACKING:
      case ContentSettingsTypes.HID_DEVICES:
      case ContentSettingsTypes.IDLE_DETECTION:
      case ContentSettingsTypes.KEYBOARD_LOCK:
      case ContentSettingsTypes.LOCAL_FONTS:
      case ContentSettingsTypes.MIC:
      case ContentSettingsTypes.MIDI_DEVICES:
      case ContentSettingsTypes.NOTIFICATIONS:
      case ContentSettingsTypes.POINTER_LOCK:
      case ContentSettingsTypes.SERIAL_PORTS:
      case ContentSettingsTypes.SMART_CARD_READERS:
      case ContentSettingsTypes.STORAGE_ACCESS:
      case ContentSettingsTypes.USB_DEVICES:
      case ContentSettingsTypes.VR:
      case ContentSettingsTypes.WINDOW_MANAGEMENT:
      case ContentSettingsTypes.WEB_APP_INSTALLATION:
      case ContentSettingsTypes.WEB_PRINTING:
        // "Ask" vs "Blocked".
        return ContentSetting.ASK;
      default:
        assertNotReached('Invalid category: ' + this.category);
    }
  }

  private getEnabledButtonClass_(): string {
    return this.allowOptionSubLabel ? 'two-line' : '';
  }

  private getDisabledButtonClass_(): string {
    return this.blockOptionSubLabel ? 'two-line' : '';
  }

  /**
   * A handler for changing the default permission value for a content type.
   * This is also called during page setup after we get the default state.
   */
  private onSelectedChanged_() {
    assert(
        this.pref_.enforcement !== chrome.settingsPrivate.Enforcement.ENFORCED);

    const allowOption =
        /** @type {!ContentSetting} */ (this.getAllowOptionForCategory_());
    this.browserProxy.setDefaultValueForContentType(
        this.category,
        this.categoryEnabled_ ? allowOption : ContentSetting.BLOCK);
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
      }
      this.set('pref_.controlledBy', controlledBy);
    } else {
      this.set('pref_.enforcement', undefined);
      this.set('pref_.controlledBy', undefined);
    }

    const enabled = this.computeIsSettingEnabled(update.setting);
    const prefValue = enabled ? SiteContentRadioSetting.ENABLED :
                                SiteContentRadioSetting.DISABLED;

    this.set('pref_.value', prefValue);
  }

  private async onCategoryChanged_(category: ContentSettingsTypes) {
    if (category !== this.category) {
      return;
    }
    const defaultValue =
        await this.browserProxy.getDefaultValueForContentType(this.category);
    this.updatePref_(defaultValue);
  }

  private get categoryEnabled_(): boolean {
    return this.pref_.value === SiteContentRadioSetting.ENABLED;
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
