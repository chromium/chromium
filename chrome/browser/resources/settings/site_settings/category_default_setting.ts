// Copyright 2016 The Chromium Authors
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
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './category_default_setting.html.js';
import {ContentSetting, ContentSettingsTypes} from './constants.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';
import {ContentSettingProvider, DefaultContentSetting} from './site_settings_prefs_browser_proxy.js';

/**
 * The setting to display as a sub-option, if any.
 */
enum SubOptionMode {
  PREF = 'pref',
  NONE = 'none',
}

export interface CategoryDefaultSettingElement {
  $: {
    toggle: SettingsToggleButtonElement,
  };
}

const CategoryDefaultSettingElementBase =
    WebUiListenerMixin(SiteSettingsMixin(PolymerElement));

export class CategoryDefaultSettingElement extends
    CategoryDefaultSettingElementBase {
  static get is() {
    return 'category-default-setting';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {

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

      controlParams_: {
        type: Object,
        value() {
          return {};
        },
      },

      /**
       * The label to be shown next to the toggle (above
       * |optionDescription_|). This will be either toggleOffLabel or
       * toggleOnLabel.
       */
      optionLabel_: String,

      /* The second line, shown under the |optionLabel_| line. This will be
       * either toggleOffDescription or toggleOnDescription. (optional)
       */
      optionDescription_: String,

      priorDefaultContentSetting_: {
        type: Object,
        value() {
          return {};
        },
      },
    };
  }

  static get observers() {
    return [
      'onCategoryChanged_(category)',
      'onChangePermissionControl_(category, controlParams_.value)',
    ];
  }

  toggleOffLabel: string;
  toggleOnLabel: string;
  toggleOffDescription: string;
  toggleOnDescription: string;
  subOptionLabel: string;
  subOptionDescription: string;
  subOptionMode: string;
  subOptionPref: boolean;
  private controlParams_: chrome.settingsPrivate.PrefObject;
  private optionLabel_: string;
  private optionDescription_: string;
  private priorDefaultContentSetting_: DefaultContentSetting;

  override ready() {
    super.ready();

    this.addWebUiListener(
        'contentSettingCategoryChanged', () => this.onCategoryChanged_());
  }

  get categoryEnabled(): boolean {
    assert(this.controlParams_);
    return !!this.controlParams_.value;
  }

  /**
   * A handler for changing the default permission value for a content type.
   * This is also called during page setup after we get the default state.
   */
  private onChangePermissionControl_() {
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
    /**
     * This list must be kept in sync with the list in
     * settings_category_default_radio_group.js
     */
    switch (this.category) {
      case ContentSettingsTypes.ADS:
      case ContentSettingsTypes.BACKGROUND_SYNC:
      case ContentSettingsTypes.FEDERATED_IDENTITY_API:
      case ContentSettingsTypes.IMAGES:
      case ContentSettingsTypes.JAVASCRIPT:
      case ContentSettingsTypes.MIXEDSCRIPT:
      case ContentSettingsTypes.PAYMENT_HANDLER:
      case ContentSettingsTypes.POPUPS:
      case ContentSettingsTypes.PROTECTED_CONTENT:
      case ContentSettingsTypes.PROTOCOL_HANDLERS:
      case ContentSettingsTypes.SENSORS:
      case ContentSettingsTypes.SOUND:
        // "Allowed" vs "Blocked".
        this.browserProxy.setDefaultValueForContentType(
            this.category,
            this.categoryEnabled ? ContentSetting.ALLOW : ContentSetting.BLOCK);
        break;
      case ContentSettingsTypes.AR:
      case ContentSettingsTypes.AUTOMATIC_DOWNLOADS:
      case ContentSettingsTypes.BLUETOOTH_DEVICES:
      case ContentSettingsTypes.BLUETOOTH_SCANNING:
      case ContentSettingsTypes.CAMERA:
      case ContentSettingsTypes.CLIPBOARD:
      case ContentSettingsTypes.FILE_SYSTEM_WRITE:
      case ContentSettingsTypes.GEOLOCATION:
      case ContentSettingsTypes.HID_DEVICES:
      case ContentSettingsTypes.IDLE_DETECTION:
      case ContentSettingsTypes.LOCAL_FONTS:
      case ContentSettingsTypes.MIC:
      case ContentSettingsTypes.MIDI_DEVICES:
      case ContentSettingsTypes.NOTIFICATIONS:
      case ContentSettingsTypes.SERIAL_PORTS:
      case ContentSettingsTypes.USB_DEVICES:
      case ContentSettingsTypes.VR:
      case ContentSettingsTypes.WINDOW_MANAGEMENT:
        // "Ask" vs "Blocked".
        this.browserProxy.setDefaultValueForContentType(
            this.category,
            this.categoryEnabled ? ContentSetting.ASK : ContentSetting.BLOCK);
        break;
      default:
        assertNotReached('Invalid category: ' + this.category);
    }
  }

  /**
   * Update the control parameter values from the content settings.
   */
  private updateControlParams_(update: DefaultContentSetting) {
    // Early out if there is no actual change.
    if (this.priorDefaultContentSetting_.setting === update.setting &&
        this.priorDefaultContentSetting_.source === update.source) {
      return;
    }
    this.priorDefaultContentSetting_ = update;

    const basePref: chrome.settingsPrivate.PrefObject = {
      key: 'controlParams',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
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
    this.controlParams_ = Object.assign(basePref, {value: prefValue});
  }

  /**
   * Handles changes to the category pref and the |category| member variable.
   */
  private onCategoryChanged_() {
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
  }

  private isToggleDisabled_(): boolean {
    return this.category === ContentSettingsTypes.POPUPS &&
        loadTimeData.getBoolean('isGuest');
  }

  private showPrefSubOption_(subOptionMode: SubOptionMode): boolean {
    return (subOptionMode === SubOptionMode.PREF);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'category-default-setting': CategoryDefaultSettingElement;
  }
}


customElements.define(
    CategoryDefaultSettingElement.is, CategoryDefaultSettingElement);
