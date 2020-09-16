// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'category-default-setting' is the polymer element for showing a certain
 * category under Site Settings.
 *
 * |optionLabel_| toggle is enabled:
 * +-------------------------------------------------+
 * | Category                                        |<-- Not defined here
 * |                                                 |
 * |  optionLabel_                     ( O)          |
 * |  optionDescription_                             |
 * |                                                 |
 * |  subOptionLabel                   ( O)          |<-- SubOptionMode.PREF
 * |  subOptionDescription                           |    (optional)
 * |                                                 |
 * +-------------------------------------------------+
 *
 * |optionLabel_| toggle is disabled:
 * +-------------------------------------------------+
 * | Category                                        |<-- Not defined here
 * |                                                 |
 * |  optionLabel_                     (O )          |
 * |  optionDescription_                             |
 * |                                                 |
 * |  subOptionLabel                   (O )          |<-- Toggle is off and
 * |  subOptionDescription                           |    disabled; or hidden
 * |                                                 |
 * +-------------------------------------------------+
 *
 * TODO(crbug.com/1113642): Remove this element when content settings redesign
 * is launched.
 */
import '../controls/settings_toggle_button.m.js';
import '../settings_shared_css.m.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';

import {ContentSetting, ContentSettingsTypes} from './constants.js';
import {SiteSettingsBehavior} from './site_settings_behavior.js';
import {ContentSettingProvider, DefaultContentSetting} from './site_settings_prefs_browser_proxy.js';

/**
 * The setting to display as a sub-option, if any.
 * @enum {string}
 */
const SubOptionMode = {
  PREF: 'pref',
  NONE: 'none',
};

Polymer({
  is: 'category-default-setting',

  _template: html`{__html_template__}`,

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

  properties: {

    /* The on/off text for |optionLabel_| below. */
    toggleOffLabel: String,
    toggleOnLabel: String,

    /* The on/off text for |optionDescription_| below. */
    toggleOffDescription: String,
    toggleOnDescription: String,

    /* The sub-option is a separate toggle. Setting this label will show the
     * additional sub option. Shown above |subOptionDescription|. (optional)
     */
    subOptionLabel: String,

    /* The second line, shown under the |subOptionLabel| line. (optional) */
    subOptionDescription: String,

    /* The valid sub-option modes. */
    subOptionMode: String,

    /* The pref that the sub-option state is bound to, when |subOptionMode| is
     * set to SubOptionMode.PREF. */
    subOptionPref: Boolean,

    /* Pref based
    /** @private {chrome.settingsPrivate.PrefObject} */
    controlParams_: {
      type: Object,
      value() {
        return /** @type {chrome.settingsPrivate.PrefObject} */ ({});
      },
    },

    /**
     * The label to be shown next to the toggle (above
     * |optionDescription_|). This will be either toggleOffLabel or
     * toggleOnLabel.
     * @private
     */
    optionLabel_: String,

    /* The second line, shown under the |optionLabel_| line. This will be
     * either toggleOffDescription or toggleOnDescription. (optional)
     * @private
     */
    optionDescription_: String,

    /** @private {!DefaultContentSetting} */
    priorDefaultContentSetting_: {
      type: Object,
      value() {
        return /** @type {DefaultContentSetting} */ ({});
      },
    },
  },

  observers: [
    'onCategoryChanged_(category)',
    'onChangePermissionControl_(category, controlParams_.value)',
  ],

  /** @override */
  ready() {
    this.addWebUIListener(
        'contentSettingCategoryChanged', this.onCategoryChanged_.bind(this));
  },

  /** @return {boolean} */
  get categoryEnabled() {
    return !!assert(this.controlParams_).value;
  },

  /**
   * A handler for changing the default permission value for a content type.
   * This is also called during page setup after we get the default state.
   * @private
   */
  onChangePermissionControl_() {
    if (this.category === undefined ||
        this.controlParams_.value === undefined) {
      // Do nothing unless all dependencies are defined.
      return;
    }

    // Don't override user settings with enforced settings.
    if (this.controlParams_.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED) {
      return;
    }
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
        this.browserProxy.setDefaultValueForContentType(
            this.category,
            this.categoryEnabled ? ContentSetting.ALLOW : ContentSetting.BLOCK);
        break;
      case ContentSettingsTypes.AUTOMATIC_DOWNLOADS:
      case ContentSettingsTypes.CAMERA:
      case ContentSettingsTypes.CLIPBOARD:
      case ContentSettingsTypes.FONT_ACCESS:
      case ContentSettingsTypes.GEOLOCATION:
      case ContentSettingsTypes.MIC:
      case ContentSettingsTypes.NOTIFICATIONS:
      case ContentSettingsTypes.UNSANDBOXED_PLUGINS:
      case ContentSettingsTypes.MIDI_DEVICES:
      case ContentSettingsTypes.USB_DEVICES:
      case ContentSettingsTypes.SERIAL_PORTS:
      case ContentSettingsTypes.BLUETOOTH_DEVICES:
      case ContentSettingsTypes.BLUETOOTH_SCANNING:
      case ContentSettingsTypes.FILE_SYSTEM_WRITE:
      case ContentSettingsTypes.HID_DEVICES:
      case ContentSettingsTypes.VR:
      case ContentSettingsTypes.AR:
      case ContentSettingsTypes.WINDOW_PLACEMENT:
      case ContentSettingsTypes.IDLE_DETECTION:
        // "Ask" vs "Blocked".
        this.browserProxy.setDefaultValueForContentType(
            this.category,
            this.categoryEnabled ? ContentSetting.ASK : ContentSetting.BLOCK);
        break;
      case ContentSettingsTypes.PLUGINS:
        // "Run important content" vs. "Block".
        this.browserProxy.setDefaultValueForContentType(
            this.category,
            this.categoryEnabled ? ContentSetting.IMPORTANT_CONTENT :
                                   ContentSetting.BLOCK);
        break;
      default:
        assertNotReached('Invalid category: ' + this.category);
    }
  },

  /**
   * Update the control parameter values from the content settings.
   * @param {!DefaultContentSetting} update
   * @private
   */
  updateControlParams_(update) {
    // Early out if there is no actual change.
    if (this.priorDefaultContentSetting_.setting === update.setting &&
        this.priorDefaultContentSetting_.source === update.source) {
      return;
    }
    this.priorDefaultContentSetting_ = update;

    const basePref = {
      'key': 'controlParams',
      'type': chrome.settingsPrivate.PrefType.BOOLEAN,
    };
    if (update.source !== undefined &&
        update.source !== ContentSettingProvider.PREFERENCE) {
      basePref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      switch (update.source) {
        case ContentSettingProvider.POLICY:
          basePref.controlledBy =
              chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;
          break;
        case ContentSettingProvider.SUPERVISED_USER:
          basePref.controlledBy = chrome.settingsPrivate.ControlledBy.PARENT;
          break;
        case ContentSettingProvider.EXTENSION:
          basePref.controlledBy = chrome.settingsPrivate.ControlledBy.EXTENSION;
          break;
        default:
          basePref.controlledBy =
              chrome.settingsPrivate.ControlledBy.USER_POLICY;
          break;
      }
    }

    const prefValue = this.computeIsSettingEnabled(update.setting);
    // The controlParams_ must be replaced (rather than just value changes) so
    // that observers will be notified of the change.
    this.controlParams_ = /** @type {chrome.settingsPrivate.PrefObject} */ (
        Object.assign({'value': prefValue}, basePref));
  },

  /**
   * Handles changes to the category pref and the |category| member variable.
   * @private
   */
  onCategoryChanged_() {
    this.browserProxy.getDefaultValueForContentType(this.category)
        .then(defaultValue => {
          this.updateControlParams_(defaultValue);

          const categoryEnabled =
              this.computeIsSettingEnabled(defaultValue.setting);
          this.optionLabel_ =
              categoryEnabled ? this.toggleOnLabel : this.toggleOffLabel;
          this.optionDescription_ = categoryEnabled ? this.toggleOnDescription :
                                                      this.toggleOffDescription;
        });
  },

  /**
   * @return {boolean}
   * @private
   */
  isToggleDisabled_() {
    return this.category === ContentSettingsTypes.POPUPS &&
        loadTimeData.getBoolean('isGuest');
  },

  /**
   * @return {boolean}
   * @private
   */
  showPrefSubOption_(subOptionMode) {
    return (subOptionMode === SubOptionMode.PREF);
  },
});
