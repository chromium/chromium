// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-manage-a11y-page' is the subpage with the accessibility
 * settings.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../../controls/settings_slider.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared_css.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {BatteryStatus, DevicePageBrowserProxy, DevicePageBrowserProxyImpl, ExternalStorage, getDisplayApi, IdleBehavior, LidClosedBehavior, NoteAppInfo, NoteAppLockScreenSupport, PowerManagementSettings, PowerSource, StorageSpaceState} from '../device_page/device_page_browser_proxy.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';
import {RouteOriginBehavior, RouteOriginBehaviorImpl, RouteOriginBehaviorInterface} from '../route_origin_behavior.js';

import {ManageA11yPageBrowserProxy, ManageA11yPageBrowserProxyImpl} from './manage_a11y_page_browser_proxy.js';

/** @const {number} */
const DEFAULT_BLACK_CURSOR_COLOR = 0;


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {RouteOriginBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsManageA11YPageElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior, I18nBehavior, RouteObserverBehavior,
      RouteOriginBehavior, WebUIListenerBehavior
    ],
    PolymerElement);

/** @polymer */
class SettingsManageA11YPageElement extends SettingsManageA11YPageElementBase {
  static get is() {
    return 'settings-manage-a11y-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * Enum values for the
       * 'settings.a11y.screen_magnifier_mouse_following_mode' preference. These
       * values map to AccessibilityController::MagnifierMouseFollowingMode, and
       * are written to prefs and metrics, so order should not be changed.
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
          // These values correspond to the i18n values in
          // settings_strings.grdp. If these values get changed then those
          // strings need to be changed as well.
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
          // These values correspond to the i18n values in
          // settings_strings.grdp. If these values get changed then those
          // strings need to be changed as well.
          return [
            {
              value: 600,
              name: loadTimeData.getString('delayBeforeClickExtremelyShort')
            },
            {
              value: 800,
              name: loadTimeData.getString('delayBeforeClickVeryShort')
            },
            {
              value: 1000,
              name: loadTimeData.getString('delayBeforeClickShort')
            },
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
              name:
                  loadTimeData.getString('autoclickMovementThresholdExtraSmall')
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
              name:
                  loadTimeData.getString('autoclickMovementThresholdExtraLarge')
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

      /** @private */
      dictationSubtitle_: {
        type: String,
        value() {
          return loadTimeData.getString('dictationDescription');
        }
      },

      /** @private */
      dictationLocaleSubtitleOverride_: {
        type: String,
        value: '',
      },

      /** @private */
      useDictationLocaleSubtitleOverride_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      dictationLocaleMenuSubtitle_: {
        type: String,
        computed: 'computeDictationLocaleSubtitle_(' +
            'dictationLocaleOptions_, ' +
            'prefs.settings.a11y.dictation_locale.value, ' +
            'dictationLocaleSubtitleOverride_)',
      },

      /** @private */
      dictationLocaleOptions_: {
        type: Array,
        value() {
          return [];
        }
      },

      /** @private */
      dictationLocalesList_: {
        type: Array,
        value() {
          return [];
        }
      },

      /** @private */
      showDictationLocaleMenu_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      dictationLearnMoreUrl_: {
        type: String,
        value: 'https://support.google.com/chromebook?p=text_dictation_m100',
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
       * Boolean indicating whether shelf navigation buttons should implicitly
       * be enabled in tablet mode - the navigation buttons are implicitly
       * enabled when spoken feedback, automatic clicks, or switch access are
       * enabled. The buttons can also be explicitly enabled by a designated
       * a11y setting.
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
       * The effective pref value that indicates whether shelf navigation
       * buttons are enabled in tablet mode.
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
          chromeos.settings.mojom.Setting
              .kFullscreenMagnifierMouseFollowingMode,
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
    };
  }

  static get observers() {
    return [
      'pointersChanged_(hasMouse_, hasPointingStick_, hasTouchpad_, ' +
          'isKioskModeActive_)',
    ];
  }

  /** @override */
  constructor() {
    super();

    /** RouteOriginBehavior override */
    this.route_ = routes.MANAGE_ACCESSIBILITY;

    /** @private {!ManageA11yPageBrowserProxy} */
    this.manageBrowserProxy_ = ManageA11yPageBrowserProxyImpl.getInstance();

    /** @private {!DevicePageBrowserProxy} */
    this.deviceBrowserProxy_ = DevicePageBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.addWebUIListener(
        'has-mouse-changed', (exists) => this.set('hasMouse_', exists));
    this.addWebUIListener(
        'has-pointing-stick-changed',
        (exists) => this.set('hasPointingStick_', exists));
    this.addWebUIListener(
        'has-touchpad-changed', (exists) => this.set('hasTouchpad_', exists));
    this.deviceBrowserProxy_.initializePointers();
    this.addWebUIListener(
        'has-hardware-keyboard',
        (hasKeyboard) => this.set('hasKeyboard_', hasKeyboard));
    this.deviceBrowserProxy_.initializeKeyboardWatcher();
  }

  /** @override */
  ready() {
    super.ready();

    this.addWebUIListener(
        'initial-data-ready',
        (startup_sound_enabled) =>
            this.onManageAllyPageReady_(startup_sound_enabled));
    this.addWebUIListener(
        'dictation-locale-menu-subtitle-changed',
        (result) => this.onDictationLocaleMenuSubtitleChanged_(result));
    this.addWebUIListener(
        'dictation-locales-set',
        (locales) => this.onDictationLocalesSet_(locales));
    this.manageBrowserProxy_.manageA11yPageReady();

    const r = routes;
    this.addFocusConfig(r.MANAGE_TTS_SETTINGS, '#ttsSubpageButton');
    this.addFocusConfig(r.MANAGE_CAPTION_SETTINGS, '#captionsSubpageButton');
    this.addFocusConfig(
        r.MANAGE_SWITCH_ACCESS_SETTINGS, '#switchAccessSubpageButton');
    this.addFocusConfig(r.DISPLAY, '#displaySubpageButton');
    this.addFocusConfig(r.KEYBOARD, '#keyboardSubpageButton');
    this.addFocusConfig(r.POINTERS, '#pointerSubpageButton');
  }

  /**
   * Note: Overrides RouteOriginBehavior implementation
   * @param {!Route} newRoute
   * @param {!Route=} prevRoute
   * @protected
   */
  currentRouteChanged(newRoute, prevRoute) {
    RouteOriginBehaviorImpl.currentRouteChanged.call(this, newRoute, prevRoute);

    // Does not apply to this page.
    if (newRoute !== routes.MANAGE_ACCESSIBILITY) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * @param {boolean} hasMouse
   * @param {boolean} hasPointingStick
   * @param {boolean} hasTouchpad
   * @private
   */
  pointersChanged_(hasMouse, hasTouchpad, hasPointingStick, isKioskModeActive) {
    this.$.pointerSubpageButton.hidden =
        (!hasMouse && !hasPointingStick && !hasTouchpad) || isKioskModeActive;
  }

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
  }

  /**
   * @param {!CustomEvent<boolean>} e
   * @private
   */
  toggleStartupSoundEnabled_(e) {
    this.manageBrowserProxy_.setStartupSoundEnabled(e.detail);
  }

  /** @private */
  onManageTtsSettingsTap_() {
    Router.getInstance().navigateTo(routes.MANAGE_TTS_SETTINGS);
  }

  /** @private */
  onChromeVoxSettingsTap_() {
    this.manageBrowserProxy_.showChromeVoxSettings();
  }

  /** @private */
  onChromeVoxTutorialTap_() {
    this.manageBrowserProxy_.showChromeVoxTutorial();
  }

  /** @private */
  onCaptionsClick_() {
    Router.getInstance().navigateTo(routes.MANAGE_CAPTION_SETTINGS);
  }

  /** @private */
  onSelectToSpeakSettingsTap_() {
    this.manageBrowserProxy_.showSelectToSpeakSettings();
  }

  /** @private */
  onSwitchAccessSettingsTap_() {
    Router.getInstance().navigateTo(routes.MANAGE_SWITCH_ACCESS_SETTINGS);
  }

  /** @private */
  onDisplayTap_() {
    Router.getInstance().navigateTo(
        routes.DISPLAY,
        /* dynamicParams */ null, /* removeSearch */ true);
  }

  /** @private */
  onAppearanceTap_() {
    // Open browser appearance section in a new browser tab.
    window.open('chrome://settings/appearance');
  }

  /** @private */
  onKeyboardTap_() {
    Router.getInstance().navigateTo(
        routes.KEYBOARD,
        /* dynamicParams */ null, /* removeSearch */ true);
  }

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
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShowShelfNavigationButtonsSettings_() {
    return !this.isKioskModeActive_ &&
        loadTimeData.getBoolean('showTabletModeShelfNavigationButtonsSettings');
  }

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
  }

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
  }

  /** @private */
  onShelfNavigationButtonsLearnMoreClicked_() {
    chrome.metricsPrivate.recordUserAction(
        'Settings_A11y_ShelfNavigationButtonsLearnMoreClicked');
  }

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

    const enabled =
        this.shadowRoot.querySelector('#shelfNavigationButtonsEnabledControl')
            .checked;
    this.set(
        'prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled.value',
        enabled);
    this.manageBrowserProxy_.recordSelectedShowShelfNavigationButtonValue(
        enabled);
  }

  /** @private */
  onA11yCursorColorChange_() {
    // Custom cursor color is enabled when the color is not set to black.
    const a11yCursorColorOn =
        this.get('prefs.settings.a11y.cursor_color.value') !==
        DEFAULT_BLACK_CURSOR_COLOR;
    this.set(
        'prefs.settings.a11y.cursor_color_enabled.value', a11yCursorColorOn);
  }


  /** @private */
  onMouseTap_() {
    Router.getInstance().navigateTo(
        routes.POINTERS,
        /* dynamicParams */ null, /* removeSearch */ true);
  }

  /**
   * Handles updating the visibility of the shelf navigation buttons setting
   * and updating whether startupSoundEnabled is checked.
   * @param {boolean} startup_sound_enabled Whether startup sound is enabled.
   * @private
   */
  onManageAllyPageReady_(startup_sound_enabled) {
    this.$.startupSoundEnabled.checked = startup_sound_enabled;
  }

  /**
   * Whether additional features link should be shown.
   * @param {boolean} isKiosk
   * @param {boolean} isGuest
   * @return {boolean}
   * @private
   */
  shouldShowAdditionalFeaturesLink_(isKiosk, isGuest) {
    return !isKiosk && !isGuest;
  }

  /**
   * @param {string} subtitle
   * @private
   */
  onDictationLocaleMenuSubtitleChanged_(subtitle) {
    this.useDictationLocaleSubtitleOverride_ = true;
    this.dictationLocaleSubtitleOverride_ = subtitle;
  }


  /**
   * Saves a list of locales and updates the UI to reflect the list.
   * @param {!Array<!Array<string>>} locales
   * @private
   */
  onDictationLocalesSet_(locales) {
    this.dictationLocalesList_ = locales;
    this.onDictationLocalesChanged_();
  }

  /**
   * Converts an array of locales and their human-readable equivalents to
   * an array of menu options.
   * TODO(crbug.com/1195916): Use 'offline' to indicate to the user which
   * locales work offline with an icon in the select options.
   * @private
   */
  onDictationLocalesChanged_() {
    const currentLocale =
        this.get('prefs.settings.a11y.dictation_locale.value');
    this.dictationLocaleOptions_ =
        this.dictationLocalesList_.map((localeInfo) => {
          return {
            name: localeInfo.name,
            value: localeInfo.value,
            worksOffline: localeInfo.worksOffline,
            installed: localeInfo.installed,
            recommended:
                localeInfo.recommended || localeInfo.value === currentLocale,
          };
        });
  }

  /**
   * Calculates the Dictation locale subtitle based on the current
   * locale from prefs and the offline availability of that locale.
   * @return {string}
   * @private
   */
  computeDictationLocaleSubtitle_() {
    if (this.useDictationLocaleSubtitleOverride_) {
      // Only use the subtitle override once, since we still want the subtitle
      // to repsond to changes to the dictation locale.
      this.useDictationLocaleSubtitleOverride_ = false;
      return this.dictationLocaleSubtitleOverride_;
    }

    const currentLocale =
        this.get('prefs.settings.a11y.dictation_locale.value');
    const locale = this.dictationLocaleOptions_.find(
        (element) => element.value === currentLocale);
    if (!locale) {
      return '';
    }

    if (!locale.worksOffline) {
      // If a locale is not supported offline, then use the network subtitle.
      return this.i18n('dictationLocaleSubLabelNetwork', locale.name);
    }

    if (!locale.installed) {
      // If a locale is supported offline, but isn't installed, then use the
      // temporary network subtitle.
      return this.i18n(
          'dictationLocaleSubLabelNetworkTemporarily', locale.name);
    }

    // If we get here, we know a locale is both supported offline and installed.
    return this.i18n('dictationLocaleSubLabelOffline', locale.name);
  }

  /** @private */
  onChangeDictationLocaleButtonClicked_() {
    this.showDictationLocaleMenu_ = true;
  }

  /** @private */
  onChangeDictationLocalesDialogClosed_() {
    this.showDictationLocaleMenu_ = false;
  }
}

customElements.define(
    SettingsManageA11YPageElement.is, SettingsManageA11YPageElement);
