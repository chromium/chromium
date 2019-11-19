// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-site-settings-page' is the settings page containing privacy and
 * security site settings.
 */

Polymer({
  is: 'settings-site-settings-page',

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * An object to bind default values to (so they are not in the |this|
     * scope). The keys of this object are the values of the
     * settings.ContentSettingsTypes enum.
     * @private
     */
    default_: {
      type: Object,
      value: function() {
        return {};
      },
    },

    /** @private */
    isGuest_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isGuest');
      }
    },

    /** @private */
    enableSafeBrowsingSubresourceFilter_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableSafeBrowsingSubresourceFilter');
      }
    },

    /** @private */
    enableExperimentalWebPlatformFeatures_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableExperimentalWebPlatformFeatures');
      },
    },

    /** @private */
    enablePaymentHandlerContentSetting_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enablePaymentHandlerContentSetting');
      }
    },

    /** @private */
    enableInsecureContentContentSetting_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableInsecureContentContentSetting');
      }
    },

    /** @private */
    enableNativeFileSystemWriteContentSetting_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean(
            'enableNativeFileSystemWriteContentSetting');
      }
    },

    /** @type {!Map<string, (string|Function)>} */
    focusConfig: {
      type: Object,
      observer: 'focusConfigChanged_',
    },
  },

  /**
   * @param {!Map<string, string>} newConfig
   * @param {?Map<string, string>} oldConfig
   * @private
   */
  focusConfigChanged_: function(newConfig, oldConfig) {
    // focusConfig is set only once on the parent, so this observer should only
    // fire once.
    assert(!oldConfig);

    // Populate the |focusConfig| map of the parent <settings-animated-pages>
    // element, with additional entries that correspond to subpage trigger
    // elements residing in this element's Shadow DOM.
    const R = settings.routes;
    const pairs = [
      [R.SITE_SETTINGS_ADS, 'ads'],
      [R.SITE_SETTINGS_ALL, 'all-sites'],
      [R.SITE_SETTINGS_AUTOMATIC_DOWNLOADS, 'automatic-downloads'],
      [R.SITE_SETTINGS_BACKGROUND_SYNC, 'background-sync'],
      [R.SITE_SETTINGS_CAMERA, 'camera'],
      [R.SITE_SETTINGS_CLIPBOARD, 'clipboard'],
      [R.SITE_SETTINGS_COOKIES, 'cookies'],
      [R.SITE_SETTINGS_FLASH, 'flash'],
      [R.SITE_SETTINGS_HANDLERS, 'protocol-handlers'],
      [R.SITE_SETTINGS_IMAGES, 'images'],
      [R.SITE_SETTINGS_JAVASCRIPT, 'javascript'],
      [R.SITE_SETTINGS_LOCATION, 'location'],
      [R.SITE_SETTINGS_MICROPHONE, 'microphone'],
      [R.SITE_SETTINGS_MIDI_DEVICES, 'midi-devices'],
      [R.SITE_SETTINGS_NOTIFICATIONS, 'notifications'],
      [R.SITE_SETTINGS_PDF_DOCUMENTS, 'pdf-documents'],
      [R.SITE_SETTINGS_POPUPS, 'popups'],
      [R.SITE_SETTINGS_PROTECTED_CONTENT, 'protected-content'],
      [R.SITE_SETTINGS_SENSORS, 'sensors'],
      [R.SITE_SETTINGS_SERIAL_PORTS, 'serial-ports'],
      [R.SITE_SETTINGS_SOUND, 'sound'],
      [R.SITE_SETTINGS_UNSANDBOXED_PLUGINS, 'unsandboxed-plugins'],
      [R.SITE_SETTINGS_USB_DEVICES, 'usb-devices'],
      [R.SITE_SETTINGS_ZOOM_LEVELS, 'zoom-levels'],
    ];

    if (this.enablePaymentHandlerContentSetting_) {
      pairs.push([R.SITE_SETTINGS_PAYMENT_HANDLER, 'paymentHandler']);
    }

    if (this.enableExperimentalWebPlatformFeatures_) {
      pairs.push([R.SITE_SETTINGS_BLUETOOTH_SCANNING, 'bluetooth-scanning']);
    }

    if (this.enableNativeFileSystemWriteContentSetting_) {
      pairs.push([
        R.SITE_SETTINGS_NATIVE_FILE_SYSTEM_WRITE, 'native-file-system-write'
      ]);
    }

    if (this.enableInsecureContentContentSetting_) {
      pairs.push([R.SITE_SETTINGS_MIXEDSCRIPT, 'mixed-script']);
    }

    pairs.forEach(([route, id]) => {
      this.focusConfig.set(route.path, () => this.async(() => {
        cr.ui.focusWithoutInk(assert(this.$$(`#${id}`)));
      }));
    });
  },

  /** @override */
  ready: function() {
    this.ContentSettingsTypes = settings.ContentSettingsTypes;
    this.ALL_SITES = settings.ALL_SITES;

    const keys = Object.keys(settings.ContentSettingsTypes);
    for (let i = 0; i < keys.length; ++i) {
      const key = settings.ContentSettingsTypes[keys[i]];
      // Default labels are not applicable to ZOOM.
      if (key == settings.ContentSettingsTypes.ZOOM_LEVELS) {
        continue;
      }
      // Protocol handlers are not available (and will DCHECK) in guest mode.
      if (this.isGuest_ &&
          key == settings.ContentSettingsTypes.PROTOCOL_HANDLERS) {
        continue;
      }
      // Similarly, protected content is only available in CrOS.
      // <if expr="not chromeos">
      if (key == settings.ContentSettingsTypes.PROTECTED_CONTENT) {
        continue;
      }
      // </if>
      this.updateDefaultValueLabel_(key);
    }

    this.addWebUIListener(
        'contentSettingCategoryChanged',
        this.updateDefaultValueLabel_.bind(this));
    this.addWebUIListener(
        'setHandlersEnabled', this.updateHandlersEnabled_.bind(this));
    this.browserProxy.observeProtocolHandlersEnabledState();
  },

  /**
   * @param {string} setting Value from settings.ContentSetting.
   * @param {string} enabled Non-block label ('feature X not allowed').
   * @param {string} disabled Block label (likely just, 'Blocked').
   * @param {?string} other Tristate value (maybe, 'session only').
   * @private
   */
  defaultSettingLabel_: function(setting, enabled, disabled, other) {
    if (setting == settings.ContentSetting.BLOCK) {
      return disabled;
    }
    if (setting == settings.ContentSetting.ALLOW) {
      return enabled;
    }
    if (other) {
      return other;
    }
    return enabled;
  },

  /**
   * @param {string} category The category to update.
   * @private
   */
  updateDefaultValueLabel_: function(category) {
    this.browserProxy.getDefaultValueForContentType(category).then(
        defaultValue => {
          this.set(
              'default_.' + Polymer.CaseMap.dashToCamelCase(category),
              defaultValue.setting);
        });
  },

  /**
   * The protocol handlers have a separate enabled/disabled notifier.
   * @param {boolean} enabled
   * @private
   */
  updateHandlersEnabled_: function(enabled) {
    const category = settings.ContentSettingsTypes.PROTOCOL_HANDLERS;
    this.set(
        'default_.' + Polymer.CaseMap.dashToCamelCase(category),
        enabled ? settings.ContentSetting.ALLOW :
                  settings.ContentSetting.BLOCK);
  },

  /**
   * Navigate to the route specified in the event dataset.
   * @param {!Event} event The tap event.
   * @private
   */
  onTapNavigate_: function(event) {
    const dataSet =
        /** @type {{route: string}} */ (event.currentTarget.dataset);
    settings.navigateTo(settings.routes[dataSet.route]);
  },
});
