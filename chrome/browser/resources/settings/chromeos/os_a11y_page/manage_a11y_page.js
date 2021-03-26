// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @const {number} */
const DEFAULT_BLACK_CURSOR_COLOR = 0;

/**
 * @fileoverview
 * 'settings-manage-a11y-page' is the subpage with the accessibility
 * settings.
 */
Polymer({
  is: 'settings-manage-a11y-page',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    settings.RouteObserverBehavior,
    settings.RouteOriginBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Enum values for the 'settings.a11y.screen_magnifier_mouse_following_mode'
     * preference. These values map to
     * AccessibilityController::MagnifierMouseFollowingMode, and are written to
     * prefs and metrics, so order should not be changed.
     * @private {!Object<string, number>}
     */
    screenMagnifierMouseFollowingModePrefValues_: {
      readOnly: true,
      type: Object,
      value: {
        CONTINUOUS: 0,
        CENTERED: 1,
        EDGE: 2,
      },
    },

    screenMagnifierZoomOptions_: {
      readOnly: true,
      type: Array,
      value() {
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
      value() {
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
      value() {
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

    /** @private {!Array<{name: string, value: number}>} */
    cursorColorOptions_: {
      readOnly: true,
      type: Array,
      value() {
        return [
          {
            value: DEFAULT_BLACK_CURSOR_COLOR,
            name: loadTimeData.getString('cursorColorBlack'),
          },
          {
            value: 0xd93025,  // Red 600
            name: loadTimeData.getString('cursorColorRed'),
          },
          {
            value: 0xf29900,  //  Yellow 700
            name: loadTimeData.getString('cursorColorYellow'),
          },
          {
            value: 0x1e8e3e,  // Green 600
            name: loadTimeData.getString('cursorColorGreen'),
          },
          {
            value: 0x03b6be,  // Cyan 600
            name: loadTimeData.getString('cursorColorCyan'),
          },
          {
            value: 0x1a73e8,  // Blue 600
            name: loadTimeData.getString('cursorColorBlue'),
          },
          {
            value: 0xc61ad9,  // Magenta 600
            name: loadTimeData.getString('cursorColorMagenta'),
          },
          {
            value: 0xf50057,  // Pink A400
            name: loadTimeData.getString('cursorColorPink'),
          },

        ];
      },
    },

    /** @private */
    isMagnifierPanningImprovementsEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isMagnifierPanningImprovementsEnabled');
      },
    },

    /** @private */
    isMagnifierContinuousMouseFollowingModeSettingEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean(
            'isMagnifierContinuousMouseFollowingModeSettingEnabled');
      },
    },

    /**
     * Whether the user is in kiosk mode.
     * @private
     */
    isKioskModeActive_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isKioskModeActive');
      }
    },

    /**
     * Whether a setting for enabling shelf navigation buttons in tablet mode
     * should be displayed in the accessibility settings.
     * @private
     */
    showShelfNavigationButtonsSettings_: {
      type: Boolean,
      computed:
          'computeShowShelfNavigationButtonsSettings_(isKioskModeActive_)',
    },

    /** @private */
    isGuest_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isGuest');
      }
    },

    /** @private */
    screenMagnifierHintLabel_: {
      type: String,
      value() {
        return this.i18n(
            'screenMagnifierHintLabel',
            this.i18n('screenMagnifierHintSearchKey'));
      }
    },

    /**
     * |hasKeyboard_|, |hasMouse_|, |hasPointingStick_|, and |hasTouchpad_|
     * start undefined so observers don't trigger until they have been
     * populated.
     * @private
     */
    hasKeyboard_: Boolean,

    /** @private */
    hasMouse_: Boolean,

    /** @private */
    hasPointingStick_: Boolean,

    /** @private */
    hasTouchpad_: Boolean,

    /**
     * Boolean indicating whether shelf navigation buttons should implicitly be
     * enabled in tablet mode - the navigation buttons are implicitly enabled
     * when spoken feedback, automatic clicks, or switch access are enabled.
     * The buttons can also be explicitly enabled by a designated a11y setting.
     * @private
     */
    shelfNavigationButtonsImplicitlyEnabled_: {
      type: Boolean,
      computed: 'computeShelfNavigationButtonsImplicitlyEnabled_(' +
          'prefs.settings.accessibility.value,' +
          'prefs.settings.a11y.autoclick.value,' +
          'prefs.settings.a11y.switch_access.enabled.value)',
    },

    /**
     * The effective pref value that indicates whether shelf navigation buttons
     * are enabled in tablet mode.
     * @type {chrome.settingsPrivate.PrefObject}
     * @private
     */
    shelfNavigationButtonsPref_: {
      type: Object,
      computed: 'getShelfNavigationButtonsEnabledPref_(' +
          'shelfNavigationButtonsImplicitlyEnabled_,' +
          'prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled)',
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kChromeVox,
        chromeos.settings.mojom.Setting.kSelectToSpeak,
        chromeos.settings.mojom.Setting.kHighContrastMode,
        chromeos.settings.mojom.Setting.kFullscreenMagnifier,
        chromeos.settings.mojom.Setting.kFullscreenMagnifierMouseFollowingMode,
        chromeos.settings.mojom.Setting.kFullscreenMagnifierFocusFollowing,
        chromeos.settings.mojom.Setting.kDockedMagnifier,
        chromeos.settings.mojom.Setting.kStickyKeys,
        chromeos.settings.mojom.Setting.kOnScreenKeyboard,
        chromeos.settings.mojom.Setting.kDictation,
        chromeos.settings.mojom.Setting.kHighlightKeyboardFocus,
        chromeos.settings.mojom.Setting.kHighlightTextCaret,
        chromeos.settings.mojom.Setting.kAutoClickWhenCursorStops,
        chromeos.settings.mojom.Setting.kLargeCursor,
        chromeos.settings.mojom.Setting.kHighlightCursorWhileMoving,
        chromeos.settings.mojom.Setting.kTabletNavigationButtons,
        chromeos.settings.mojom.Setting.kMonoAudio,
        chromeos.settings.mojom.Setting.kStartupSound,
        chromeos.settings.mojom.Setting.kEnableSwitchAccess,
        chromeos.settings.mojom.Setting.kEnableCursorColor,
      ]),
    },
  },

  observers: [
    'pointersChanged_(hasMouse_, hasPointingStick_, hasTouchpad_, ' +
        'isKioskModeActive_)',
  ],

  /** settings.RouteOriginBehavior override */
  route_: settings.routes.MANAGE_ACCESSIBILITY,

  /** @private {?ManageA11yPageBrowserProxy} */
  manageBrowserProxy_: null,

  /** @private {?settings.DevicePageBrowserProxy} */
  deviceBrowserProxy_: null,

  /** @override */
  created() {
    this.manageBrowserProxy_ = ManageA11yPageBrowserProxyImpl.getInstance();
    this.deviceBrowserProxy_ =
        settings.DevicePageBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'has-mouse-changed', this.set.bind(this, 'hasMouse_'));
    this.addWebUIListener(
        'has-pointing-stick-changed', this.set.bind(this, 'hasPointingStick_'));
    this.addWebUIListener(
        'has-touchpad-changed', this.set.bind(this, 'hasTouchpad_'));
    this.deviceBrowserProxy_.initializePointers();

    this.addWebUIListener(
        'has-hardware-keyboard', this.set.bind(this, 'hasKeyboard_'));
    this.deviceBrowserProxy_.initializeKeyboardWatcher();
  },

  /** @override */
  ready() {
    this.addWebUIListener(
        'initial-data-ready', this.onManageAllyPageReady_.bind(this));
    this.manageBrowserProxy_.manageA11yPageReady();

    const r = settings.routes;
    this.addFocusConfig_(r.MANAGE_TTS_SETTINGS, '#ttsSubpageButton');
    this.addFocusConfig_(r.MANAGE_CAPTION_SETTINGS, '#captionsSubpageButton');
    this.addFocusConfig_(
        r.MANAGE_SWITCH_ACCESS_SETTINGS, '#switchAccessSubpageButton');
    this.addFocusConfig_(r.DISPLAY, '#displaySubpageButton');
    this.addFocusConfig_(r.KEYBOARD, '#keyboardSubpageButton');
    this.addFocusConfig_(r.POINTERS, '#pointerSubpageButton');
  },

  /**
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.MANAGE_ACCESSIBILITY) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * @param {boolean} hasMouse
   * @param {boolean} hasPointingStick
   * @param {boolean} hasTouchpad
   * @private
   */
  pointersChanged_(hasMouse, hasTouchpad, hasPointingStick, isKioskModeActive) {
    this.$.pointerSubpageButton.hidden =
        (!hasMouse && !hasPointingStick && !hasTouchpad) || isKioskModeActive;
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
  getSelectToSpeakDescription_(
      enabled, hasKeyboard, disabledString, keyboardString, noKeyboardString) {
    return !enabled ? disabledString :
                      hasKeyboard ? keyboardString : noKeyboardString;
  },

  /**
   * @param {!CustomEvent<boolean>} e
   * @private
   */
  toggleStartupSoundEnabled_(e) {
    this.manageBrowserProxy_.setStartupSoundEnabled(e.detail);
  },

  /** @private */
  onManageTtsSettingsTap_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.MANAGE_TTS_SETTINGS);
  },

  /** @private */
  onChromeVoxSettingsTap_() {
    this.manageBrowserProxy_.showChromeVoxSettings();
  },

  /** @private */
  onCaptionsClick_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.MANAGE_CAPTION_SETTINGS);
  },

  /** @private */
  onSelectToSpeakSettingsTap_() {
    this.manageBrowserProxy_.showSelectToSpeakSettings();
  },

  /** @private */
  onSwitchAccessSettingsTap_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.MANAGE_SWITCH_ACCESS_SETTINGS);
  },

  /** @private */
  onDisplayTap_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.DISPLAY,
        /* dynamicParams */ null, /* removeSearch */ true);
  },

  /** @private */
  onAppearanceTap_() {
    // Open browser appearance section in a new browser tab.
    window.open('chrome://settings/appearance');
  },

  /** @private */
  onKeyboardTap_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.KEYBOARD,
        /* dynamicParams */ null, /* removeSearch */ true);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onA11yCaretBrowsingChange_(event) {
    if (event.target.checked) {
      chrome.metricsPrivate.recordUserAction(
          'Accessibility.CaretBrowsing.EnableWithSettings');
    } else {
      chrome.metricsPrivate.recordUserAction(
          'Accessibility.CaretBrowsing.DisableWithSettings');
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShowShelfNavigationButtonsSettings_() {
    return !this.isKioskModeActive_ &&
        loadTimeData.getBoolean('showTabletModeShelfNavigationButtonsSettings');
  },

  /**
   * @return {boolean} Whether shelf navigation buttons should implicitly be
   *     enabled in tablet mode (due to accessibility settings different than
   *     shelf_navigation_buttons_enabled_in_tablet_mode).
   * @private
   */
  computeShelfNavigationButtonsImplicitlyEnabled_() {
    /**
     * Gets the bool pref value for the provided pref key.
     * @param {string} key
     * @return {boolean}
     */
    const getBoolPrefValue = (key) => {
      const pref = /** @type {chrome.settingsPrivate.PrefObject} */ (
          this.get(key, this.prefs));
      return pref && !!pref.value;
    };

    return getBoolPrefValue('settings.accessibility') ||
        getBoolPrefValue('settings.a11y.autoclick') ||
        getBoolPrefValue('settings.a11y.switch_access.enabled');
  },

  /**
   * Calculates the effective value for "shelf navigation buttons enabled in
   * tablet mode" setting - if the setting is implicitly enabled (by other a11y
   * settings), this will return a stub pref value.
   * @private
   * @return {chrome.settingsPrivate.PrefObject}
   */
  getShelfNavigationButtonsEnabledPref_() {
    if (this.shelfNavigationButtonsImplicitlyEnabled_) {
      return /** @type {!chrome.settingsPrivate.PrefObject}*/ ({
        value: true,
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        key: ''
      });
    }

    return /** @type {chrome.settingsPrivate.PrefObject} */ (this.get(
        'settings.a11y.tablet_mode_shelf_nav_buttons_enabled', this.prefs));
  },

  /** @private */
  onShelfNavigationButtonsLearnMoreClicked_() {
    chrome.metricsPrivate.recordUserAction(
        'Settings_A11y_ShelfNavigationButtonsLearnMoreClicked');
  },

  /**
   * Handles the <code>tablet_mode_shelf_nav_buttons_enabled</code> setting's
   * toggle changes. It updates the backing pref value, unless the setting is
   * implicitly enabled.
   * @private
   */
  updateShelfNavigationButtonsEnabledPref_() {
    if (this.shelfNavigationButtonsImplicitlyEnabled_) {
      return;
    }

    const enabled = this.$$('#shelfNavigationButtonsEnabledControl').checked;
    this.set(
        'prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled.value',
        enabled);
    this.manageBrowserProxy_.recordSelectedShowShelfNavigationButtonValue(
        enabled);
  },

  /** @private */
  onA11yCursorColorChange_() {
    // Custom cursor color is enabled when the color is not set to black.
    const a11yCursorColorOn =
        this.get('prefs.settings.a11y.cursor_color.value') !==
        DEFAULT_BLACK_CURSOR_COLOR;
    this.set(
        'prefs.settings.a11y.cursor_color_enabled.value', a11yCursorColorOn);
  },


  /** @private */
  onMouseTap_() {
    settings.Router.getInstance().navigateTo(
        settings.routes.POINTERS,
        /* dynamicParams */ null, /* removeSearch */ true);
  },

  /**
   * Handles updating the visibility of the shelf navigation buttons setting
   * and updating whether startupSoundEnabled is checked.
   * @param {boolean} startup_sound_enabled Whether startup sound is enabled.
   * @private
   */
  onManageAllyPageReady_(startup_sound_enabled) {
    this.$.startupSoundEnabled.checked = startup_sound_enabled;
  },
  /*
   * Whether additional features link should be shown.
   * @param {boolean} isKiosk
   * @param {boolean} isGuest
   * @return {boolean}
   * @private
   */
  shouldShowAdditionalFeaturesLink_(isKiosk, isGuest) {
    return !isKiosk && !isGuest;
  },
});
