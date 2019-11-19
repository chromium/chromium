// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-manage-a11y-page' is the subpage with the accessibility
 * settings.
 */
Polymer({
  is: 'settings-manage-a11y-page',

  behaviors: [WebUIListenerBehavior, settings.RouteObserverBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        const map = new Map();
        if (settings.routes.MANAGE_TTS_SETTINGS) {
          map.set(
              settings.routes.MANAGE_TTS_SETTINGS.path, '#ttsSubpageButton');
        }
        if (settings.routes.MANAGE_CAPTION_SETTINGS) {
          map.set(
              settings.routes.MANAGE_CAPTION_SETTINGS.path,
              '#captionsSubpageButton');
        }
        if (settings.routes.MANAGE_SWITCH_ACCESS_SETTINGS) {
          map.set(
              settings.routes.MANAGE_SWITCH_ACCESS_SETTINGS.path,
              '#switchAccessSubpageButton');
        }
        if (settings.routes.DISPLAY) {
          map.set(settings.routes.DISPLAY.path, '#displaySubpageButton');
        }
        if (settings.routes.APPEARANCE) {
          map.set(settings.routes.APPEARANCE.path, '#appearanceSubpageButton');
        }
        if (settings.routes.KEYBOARD) {
          map.set(settings.routes.KEYBOARD.path, '#keyboardSubpageButton');
        }
        if (settings.routes.POINTERS) {
          map.set(settings.routes.POINTERS.path, '#pointerSubpageButton');
        }
        return map;
      },
    },

    screenMagnifierZoomOptions_: {
      readOnly: true,
      type: Array,
      value: function() {
        // These values correspond to the i18n values in settings_strings.grdp.
        // If these values get changed then those strings need to be changed as
        // well.
        return [
          {value: 2, name: loadTimeData.getString('screenMagnifierZoom2x')},
          {value: 4, name: loadTimeData.getString('screenMagnifierZoom4x')},
          {value: 6, name: loadTimeData.getString('screenMagnifierZoom6x')},
          {value: 8, name: loadTimeData.getString('screenMagnifierZoom8x')},
          {value: 10, name: loadTimeData.getString('screenMagnifierZoom10x')},
          {value: 12, name: loadTimeData.getString('screenMagnifierZoom12x')},
          {value: 14, name: loadTimeData.getString('screenMagnifierZoom14x')},
          {value: 16, name: loadTimeData.getString('screenMagnifierZoom16x')},
          {value: 18, name: loadTimeData.getString('screenMagnifierZoom18x')},
          {value: 20, name: loadTimeData.getString('screenMagnifierZoom20x')},
        ];
      },
    },

    autoClickDelayOptions_: {
      readOnly: true,
      type: Array,
      value: function() {
        // These values correspond to the i18n values in settings_strings.grdp.
        // If these values get changed then those strings need to be changed as
        // well.
        return [
          {
            value: 600,
            name: loadTimeData.getString('delayBeforeClickExtremelyShort')
          },
          {
            value: 800,
            name: loadTimeData.getString('delayBeforeClickVeryShort')
          },
          {value: 1000, name: loadTimeData.getString('delayBeforeClickShort')},
          {value: 2000, name: loadTimeData.getString('delayBeforeClickLong')},
          {
            value: 4000,
            name: loadTimeData.getString('delayBeforeClickVeryLong')
          },
        ];
      },
    },

    autoClickMovementThresholdOptions_: {
      readOnly: true,
      type: Array,
      value: function() {
        return [
          {
            value: 5,
            name: loadTimeData.getString('autoclickMovementThresholdExtraSmall')
          },
          {
            value: 10,
            name: loadTimeData.getString('autoclickMovementThresholdSmall')
          },
          {
            value: 20,
            name: loadTimeData.getString('autoclickMovementThresholdDefault')
          },
          {
            value: 30,
            name: loadTimeData.getString('autoclickMovementThresholdLarge')
          },
          {
            value: 40,
            name: loadTimeData.getString('autoclickMovementThresholdExtraLarge')
          },
        ];
      },
    },

    showExperimentalSwitchAccess_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean(
            'showExperimentalAccessibilitySwitchAccess');
      },
    },

    /** @private */
    isGuest_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isGuest');
      }
    },

    /**
     * Whether this page shown as part of OS settings.
     * TODO(crbug.com/986596): Remove this when SplitSettings is the default.
     * @private
     */
    isOSSettings_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isOSSettings');
      },
    },

    /**
     * |hasKeyboard_|starts undefined so observers don't trigger
     * until it has been populated.
     * @private
     */
    hasKeyboard_: Boolean,
  },

  /**
   * The route corresponding to this page.
   * @private {!settings.Route|undefined}
   */
  route_: settings.routes.MANAGE_ACCESSIBILITY,

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'has-hardware-keyboard', this.set.bind(this, 'hasKeyboard_'));
    chrome.send('initializeKeyboardWatcher');
  },

  /** @override */
  ready: function() {
    this.addWebUIListener(
        'startup-sound-enabled-updated',
        this.updateStartupSoundEnabled_.bind(this));
    chrome.send('getStartupSoundEnabled');
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} newRoute
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged: function(newRoute, oldRoute) {
    // Don't attempt to focus any anchor element, unless last navigation was a
    // 'pop' (backwards) navigation.
    if (!settings.lastRouteChangeWasPopstate()) {
      return;
    }

    const focusSelector = this.focusConfig_.get(oldRoute.path);

    if (this.route_ != newRoute || !focusSelector) {
      return;
    }

    cr.ui.focusWithoutInk(assert(this.$$(focusSelector)));
  },

  /**
   * Updates the Select-to-Speak description text based on:
   *    1. Whether Select-to-Speak is enabled.
   *    2. If it is enabled, whether a physical keyboard is present.
   * @param {boolean} enabled
   * @param {boolean} hasKeyboard
   * @param {string} disabledString String to show when Select-to-Speak is
   *    disabled.
   * @param {string} keyboardString String to show when there is a physical
   *    keyboard
   * @param {string} noKeyboardString String to show when there is no keyboard
   * @private
   */
  getSelectToSpeakDescription_: function(
      enabled, hasKeyboard, disabledString, keyboardString, noKeyboardString) {
    return !enabled ? disabledString :
                      hasKeyboard ? keyboardString : noKeyboardString;
  },

  /**
   * @param {!CustomEvent<boolean>} e
   * @private
   */
  toggleStartupSoundEnabled_: function(e) {
    chrome.send('setStartupSoundEnabled', [e.detail]);
  },

  /**
   * @param {boolean} enabled
   * @private
   */
  updateStartupSoundEnabled_: function(enabled) {
    this.$.startupSoundEnabled.checked = enabled;
  },

  /** @private */
  onManageTtsSettingsTap_: function() {
    settings.navigateTo(settings.routes.MANAGE_TTS_SETTINGS);
  },

  /** @private */
  onChromeVoxSettingsTap_: function() {
    chrome.send('showChromeVoxSettings');
  },

  /** @private */
  onCaptionsClick_: function() {
    settings.navigateTo(settings.routes.MANAGE_CAPTION_SETTINGS);
  },

  /** @private */
  onSelectToSpeakSettingsTap_: function() {
    chrome.send('showSelectToSpeakSettings');
  },

  /** @private */
  onSwitchAccessSettingsTap_: function() {
    settings.navigateTo(settings.routes.MANAGE_SWITCH_ACCESS_SETTINGS);
  },

  /** @private */
  onDisplayTap_: function() {
    settings.navigateTo(
        settings.routes.DISPLAY,
        /* dynamicParams */ null, /* removeSearch */ true);
  },

  /** @private */
  onAppearanceTap_: function() {
    if (loadTimeData.getBoolean('isOSSettings')) {
      // Open browser appearance section in a new browser tab.
      window.open('chrome://settings/appearance');
    } else {
      // Open browser appearance in this settings window.
      // TODO(crbug.com/986596): Remove this when SplitSettings is the default.
      settings.navigateTo(
          settings.routes.APPEARANCE,
          /* dynamicParams */ null, /* removeSearch */ true);
    }
  },

  /** @private */
  onKeyboardTap_: function() {
    settings.navigateTo(
        settings.routes.KEYBOARD,
        /* dynamicParams */ null, /* removeSearch */ true);
  },

  /** @private */
  onMouseTap_: function() {
    settings.navigateTo(
        settings.routes.POINTERS,
        /* dynamicParams */ null, /* removeSearch */ true);
  },
});
