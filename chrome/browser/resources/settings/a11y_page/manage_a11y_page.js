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

  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
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

    autoClickEventTypeOptions_: {
      readOnly: true,
      type: Array,
      value: function() {
        // These values correspond to the i18n values in settings_strings.grdp
        // and the enums in accessibility_controller.mojom, AutoclickEventType.
        // If these values get changed then those strings need to be changed as
        // well.
        return [
          {
            // mojom::AutoclickEventType::kLeftClick
            value: 0,
            name: loadTimeData.getString('autoclickEventTypeLeftClick')
          },
          {
            // mojom::AutoclickEventType::kRightClick
            value: 1,
            name: loadTimeData.getString('autoclickEventTypeRightClick')
          },
          {
            // mojom::AutoclickEventType::kDragAndDrop
            value: 2,
            name: loadTimeData.getString('autoclickEventTypeDragAndDrop')
          },
          {
            // mojom::AutoclickEventType::kDoubleClick
            value: 3,
            name: loadTimeData.getString('autoclickEventTypeDoubleClick')
          },
          {
            // mojom::AutoclickEventType::kNoAction
            value: 4,
            name: loadTimeData.getString('autoclickEventTypeNoAction')
          },
        ];
      },
    },

    /**
     * Whether to show experimental accessibility features.
     * @private {boolean}
     */
    showExperimentalFeatures_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('showExperimentalA11yFeatures');
      },
    },

    /**
     * Whether the docked magnifier flag is enabled.
     * @private {boolean}
     */
    dockedMagnifierFeatureEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('dockedMagnifierFeatureEnabled');
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
     * |hasKeyboard_|starts undefined so observers don't trigger
     * until it has been populated.
     * @private
     */
    hasKeyboard_: Boolean,
  },

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
   * @param {!CustomEvent} e
   * @private
   */
  toggleStartupSoundEnabled_: function(e) {
    let checked = /** @type {boolean} */ (e.detail);
    chrome.send('setStartupSoundEnabled', [checked]);
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
  onSelectToSpeakSettingsTap_: function() {
    chrome.send('showSelectToSpeakSettings');
  },

  /** @private */
  onSwitchAccessSettingsTap_: function() {
    chrome.send('showSwitchAccessSettings');
  },

  /** @private */
  onDisplayTap_: function() {
    settings.navigateTo(
        settings.routes.DISPLAY,
        /* dynamicParams */ null, /* removeSearch */ true);
  },

  /** @private */
  onAppearanceTap_: function() {
    settings.navigateTo(
        settings.routes.APPEARANCE,
        /* dynamicParams */ null, /* removeSearch */ true);
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
