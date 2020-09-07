// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-category-default-radio-group' is the polymer element for showing
 * a certain category under Site Settings.
 */
import '../settings_shared_css.m.js';
import '../controls/settings_radio_group.m.js';
import '../privacy_page/collapse_radio_button.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {loadTimeData} from '../i18n_setup.js';

import {ContentSetting, ContentSettingsTypes} from './constants.js';
import {SiteSettingsBehavior} from './site_settings_behavior.js';
import {ContentSettingProvider, DefaultContentSetting} from './site_settings_prefs_browser_proxy.js';

/**
 * Selected content setting radio option.
 * @enum {number}
 */
export const SiteContentRadioSetting = {
  DISABLED: 0,
  ENABLED: 1,
};

Polymer({
  is: 'settings-category-default-radio-group',

  _template: html`{__html_template__}`,

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

  properties: {
    allowOptionLabel: String,
    allowOptionSubLabel: String,
    allowOptionIcon: String,

    blockOptionLabel: String,
    blockOptionSubLabel: String,
    blockOptionIcon: String,

    /** @private */
    siteContentRadioSettingEnum_: {
      type: Object,
      value: SiteContentRadioSetting,
    },

    /**
     * Preference object used to keep track of the selected content setting
     * option.
     * @private {!chrome.settingsPrivate.PrefObject}
     */
    pref_: {
      type: Object,
      value() {
        return /** @type {!chrome.settingsPrivate.PrefObject} */ ({
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: -1,  // No element is selected until the value is loaded.
        });
      },
    },
  },

  observers: [
    'onCategoryChanged_(category)',
  ],

  /** @override */
  ready() {
    this.addWebUIListener(
        'contentSettingCategoryChanged', this.onCategoryChanged_.bind(this));
  },

  /**
   * @return {!ContentSetting}
   * @private
   */
  getAllowOptionForCategory_() {
    switch (this.category) {
      case ContentSettingsTypes.ADS:
      case ContentSettingsTypes.BACKGROUND_SYNC:
      case ContentSettingsTypes.IMAGES:
      case ContentSettingsTypes.JAVASCRIPT:
      case ContentSettingsTypes.MIXEDSCRIPT:
      case ContentSettingsTypes.SOUND:
      case ContentSettingsTypes.SENSORS:
      case ContentSettingsTypes.PAYMENT_HANDLER:
      case ContentSettingsTypes.POPUPS:
      case ContentSettingsTypes.PROTOCOL_HANDLERS:
        // "Allowed" vs "Blocked".
        return ContentSetting.ALLOW;
      case ContentSettingsTypes.AUTOMATIC_DOWNLOADS:
      case ContentSettingsTypes.CAMERA:
      case ContentSettingsTypes.CLIPBOARD:
      case ContentSettingsTypes.GEOLOCATION:
      case ContentSettingsTypes.MIC:
      case ContentSettingsTypes.NOTIFICATIONS:
      case ContentSettingsTypes.UNSANDBOXED_PLUGINS:
      case ContentSettingsTypes.MIDI_DEVICES:
      case ContentSettingsTypes.USB_DEVICES:
      case ContentSettingsTypes.SERIAL_PORTS:
      case ContentSettingsTypes.BLUETOOTH_DEVICES:
      case ContentSettingsTypes.BLUETOOTH_SCANNING:
      case ContentSettingsTypes.HID_DEVICES:
      case ContentSettingsTypes.VR:
      case ContentSettingsTypes.AR:
      case ContentSettingsTypes.WINDOW_PLACEMENT:
        // "Ask" vs "Blocked".
        return ContentSetting.ASK;
      case ContentSettingsTypes.PLUGINS:
        // "Run important content" vs. "Block".
        return ContentSetting.IMPORTANT_CONTENT;
      default:
        assertNotReached('Invalid category: ' + this.category);
        return ContentSetting.ALLOW;
    }
  },

  /**
   * A handler for changing the default permission value for a content type.
   * This is also called during page setup after we get the default state.
   * @private
   */
  onSelectedChanged_() {
    assert(
        this.pref_.enforcement !== chrome.settingsPrivate.Enforcement.ENFORCED);

    const allowOption =
        /** @type {!ContentSetting} */ (this.getAllowOptionForCategory_());
    this.browserProxy.setDefaultValueForContentType(
        this.category,
        this.categoryEnabled_ ? allowOption : ContentSetting.BLOCK);
  },

  /**
   * Update the pref values from the content settings.
   * @param {!DefaultContentSetting} update The updated content setting value.
   * @private
   */
  updatePref_(update) {
    if (update.source !== undefined &&
        update.source !== ContentSettingProvider.PREFERENCE) {
      this.set(
          'pref_.enforcement', chrome.settingsPrivate.Enforcement.ENFORCED);
      let controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
      switch (update.source) {
        case ContentSettingProvider.POLICY:
          controlledBy = chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;
          break;
        case ContentSettingProvider.SUPERVISED_USER:
          controlledBy = chrome.settingsPrivate.ControlledBy.PARENT;
          break;
        case ContentSettingProvider.EXTENSION:
          controlledBy = chrome.settingsPrivate.ControlledBy.EXTENSION;
          break;
      }
      this.set('pref_.controlledBy', controlledBy);
    }

    const enabled = this.computeIsSettingEnabled(update.setting);
    const prefValue = enabled ? this.siteContentRadioSettingEnum_.ENABLED :
                                this.siteContentRadioSettingEnum_.DISABLED;

    this.set('pref_.value', prefValue);
  },

  /** @private */
  async onCategoryChanged_(category) {
    if (category !== this.category) {
      return;
    }
    const defaultValue =
        await this.browserProxy.getDefaultValueForContentType(this.category);
    this.updatePref_(defaultValue);
  },

  /**
   * @return {boolean}
   * @private
   */
  get categoryEnabled_() {
    return this.pref_.value === SiteContentRadioSetting.ENABLED;
  },

  /**
   * Check if the category is popups and the user is logged in guest mode.
   * Users in guest mode are not allowed to modify pop-ups content setting.
   * @return {boolean}
   * @private
   */
  isRadioGroupDisabled_() {
    return this.category === ContentSettingsTypes.POPUPS &&
        loadTimeData.getBoolean('isGuest');
  },
});
