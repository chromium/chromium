// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'category-default-setting' is the polymer element for showing a certain
 * category under Site Settings.
 */
Polymer({
  is: 'category-default-setting',

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

  properties: {
    /* The second line, shown under the |optionLabel_| line. (optional) */
    optionDescription: String,

    /* The second line, shown under the |subOptionLabel| line. (optional) */
    subOptionDescription: String,

    /* The sub-option is a separate toggle. Setting this label will show the
     * additional toggle. Shown above |subOptionDescription|. (optional) */
    subOptionLabel: String,

    /* The on/off text for |optionLabel_| below. */
    toggleOffLabel: String,
    toggleOnLabel: String,

    /** @private {chrome.settingsPrivate.PrefObject} */
    controlParams_: {
      type: Object,
      value: function() {
        return /** @type {chrome.settingsPrivate.PrefObject} */ ({});
      },
    },

    /**
     * The label to be shown next to the toggle (above |optionDescription|).
     * This will be either toggleOffLabel or toggleOnLabel.
     * @private
     */
    optionLabel_: String,

    /** @private {!DefaultContentSetting} */
    priorDefaultContentSetting_: {
      type: Object,
      value: function() {
        return /** @type {DefaultContentSetting} */ ({});
      },
    },

    /**
     * Cookies and Flash settings have a sub-control that is used to mimic a
     * tri-state value.
     * @private {chrome.settingsPrivate.PrefObject}
     */
    subControlParams_: {
      type: Object,
      value: function() {
        return /** @type {chrome.settingsPrivate.PrefObject} */ ({});
      },
    },
  },

  observers: [
    'onCategoryChanged_(category)',
    'onChangePermissionControl_(category, controlParams_.value, ' +
        'subControlParams_.value)',
  ],

  /** @override */
  ready: function() {
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
  onChangePermissionControl_: function() {
    if (this.category === undefined ||
        this.controlParams_.value === undefined ||
        this.subControlParams_.value === undefined) {
      // Do nothing unless all dependencies are defined.
      return;
    }

    // Don't override user settings with enforced settings.
    if (this.controlParams_.enforcement ==
        chrome.settingsPrivate.Enforcement.ENFORCED) {
      return;
    }
    switch (this.category) {
      case settings.ContentSettingsTypes.ADS:
      case settings.ContentSettingsTypes.BACKGROUND_SYNC:
      case settings.ContentSettingsTypes.IMAGES:
      case settings.ContentSettingsTypes.JAVASCRIPT:
      case settings.ContentSettingsTypes.MIXEDSCRIPT:
      case settings.ContentSettingsTypes.SOUND:
      case settings.ContentSettingsTypes.SENSORS:
      case settings.ContentSettingsTypes.PAYMENT_HANDLER:
      case settings.ContentSettingsTypes.POPUPS:
      case settings.ContentSettingsTypes.PROTOCOL_HANDLERS:

        // "Allowed" vs "Blocked".
        this.browserProxy.setDefaultValueForContentType(
            this.category,
            this.categoryEnabled ? settings.ContentSetting.ALLOW :
                                   settings.ContentSetting.BLOCK);
        break;
      case settings.ContentSettingsTypes.AUTOMATIC_DOWNLOADS:
      case settings.ContentSettingsTypes.CAMERA:
      case settings.ContentSettingsTypes.CLIPBOARD:
      case settings.ContentSettingsTypes.GEOLOCATION:
      case settings.ContentSettingsTypes.MIC:
      case settings.ContentSettingsTypes.NOTIFICATIONS:
      case settings.ContentSettingsTypes.UNSANDBOXED_PLUGINS:
      case settings.ContentSettingsTypes.MIDI_DEVICES:
      case settings.ContentSettingsTypes.USB_DEVICES:
      case settings.ContentSettingsTypes.SERIAL_PORTS:
      case settings.ContentSettingsTypes.BLUETOOTH_SCANNING:
      case settings.ContentSettingsTypes.NATIVE_FILE_SYSTEM_WRITE:
        // "Ask" vs "Blocked".
        this.browserProxy.setDefaultValueForContentType(
            this.category,
            this.categoryEnabled ? settings.ContentSetting.ASK :
                                   settings.ContentSetting.BLOCK);
        break;
      case settings.ContentSettingsTypes.COOKIES:
        // This category is tri-state: "Allow", "Block", "Keep data until
        // browser quits".
        let value = settings.ContentSetting.BLOCK;
        if (this.categoryEnabled) {
          value = this.subControlParams_.value ?
              settings.ContentSetting.SESSION_ONLY :
              settings.ContentSetting.ALLOW;
        }
        this.browserProxy.setDefaultValueForContentType(this.category, value);
        break;
      case settings.ContentSettingsTypes.PLUGINS:
        // "Run important content" vs. "Block".
        this.browserProxy.setDefaultValueForContentType(
            this.category,
            this.categoryEnabled ? settings.ContentSetting.IMPORTANT_CONTENT :
                                   settings.ContentSetting.BLOCK);
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
  updateControlParams_: function(update) {
    // Early out if there is no actual change.
    if (this.priorDefaultContentSetting_.setting == update.setting &&
        this.priorDefaultContentSetting_.source == update.source) {
      return;
    }
    this.priorDefaultContentSetting_ = update;

    const basePref = {
      'key': 'controlParams',
      'type': chrome.settingsPrivate.PrefType.BOOLEAN,
    };
    if (update.source !== undefined &&
        update.source != ContentSettingProvider.PREFERENCE) {
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

    const subPrefValue =
        this.category == settings.ContentSettingsTypes.COOKIES &&
        update.setting == settings.ContentSetting.SESSION_ONLY;
    // The subControlParams_ must be replaced (rather than just value changes)
    // so that observers will be notified of the change.
    this.subControlParams_ = /** @type {chrome.settingsPrivate.PrefObject} */ (
        Object.assign({'value': subPrefValue}, basePref));
  },

  /**
   * Handles changes to the category pref and the |category| member variable.
   * @private
   */
  onCategoryChanged_: function() {
    this.browserProxy.getDefaultValueForContentType(this.category)
        .then(defaultValue => {
          this.updateControlParams_(defaultValue);

          const categoryEnabled =
              this.computeIsSettingEnabled(defaultValue.setting);
          this.optionLabel_ =
              categoryEnabled ? this.toggleOnLabel : this.toggleOffLabel;
        });
  },

  /**
   * @return {boolean}
   * @private
   */
  isToggleDisabled_: function() {
    return this.category == settings.ContentSettingsTypes.POPUPS &&
        loadTimeData.getBoolean('isGuest');
  },
});
