// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-manage-a11y-subpage' is the subpage with the accessibility
 * settings.
 */

import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '/shared/settings/controls/settings_slider.js';
import '/shared/settings/controls/settings_toggle_button.js';
import '../settings_shared.css.js';
import './change_dictation_locale_dialog.js';

import {SettingsToggleButtonElement} from '/shared/settings/controls/settings_toggle_button.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {DevicePageBrowserProxy, DevicePageBrowserProxyImpl} from '../device_page/device_page_browser_proxy.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {routes} from '../os_settings_routes.js';
import {RouteOriginMixin} from '../route_origin_mixin.js';
import {Route, Router} from '../router.js';

import {getTemplate} from './manage_a11y_subpage.html.js';
import {ManageA11ySubpageBrowserProxy, ManageA11ySubpageBrowserProxyImpl} from './manage_a11y_subpage_browser_proxy.js';

interface Option {
  name: string;
  value: number;
}

interface LocaleInfo {
  name: string;
  value: string;
  worksOffline: boolean;
  installed: boolean;
  recommended: boolean;
}

const DEFAULT_BLACK_CURSOR_COLOR: number = 0;

export interface SettingsManageA11ySubpageElement {
  $: {
    pointerSubpageButton: CrLinkRowElement,
    startupSoundEnabled: CrToggleElement,
  };
}

const SettingsManageA11ySubpageElementBase = PrefsMixin(DeepLinkingMixin(
    RouteOriginMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))));

export class SettingsManageA11ySubpageElement extends
    SettingsManageA11ySubpageElementBase {
  static get is() {
    return 'settings-manage-a11y-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Enum values for the
       * 'settings.a11y.screen_magnifier_mouse_following_mode' preference. These
       * values map to AccessibilityController::MagnifierMouseFollowingMode, and
       * are written to prefs and metrics, so order should not be changed.
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

      /**
       * Drop down menu options for auto click delay.
       */
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
              name: loadTimeData.getString('delayBeforeClickExtremelyShort'),
            },
            {
              value: 800,
              name: loadTimeData.getString('delayBeforeClickVeryShort'),
            },
            {
              value: 1000,
              name: loadTimeData.getString('delayBeforeClickShort'),
            },
            {value: 2000, name: loadTimeData.getString('delayBeforeClickLong')},
            {
              value: 4000,
              name: loadTimeData.getString('delayBeforeClickVeryLong'),
            },
          ];
        },
      },

      /**
       * Drop down menu options for auto click movement threshold.
       */
      autoClickMovementThresholdOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: 5,
              name: loadTimeData.getString(
                  'autoclickMovementThresholdExtraSmall'),
            },
            {
              value: 10,
              name: loadTimeData.getString('autoclickMovementThresholdSmall'),
            },
            {
              value: 20,
              name: loadTimeData.getString('autoclickMovementThresholdDefault'),
            },
            {
              value: 30,
              name: loadTimeData.getString('autoclickMovementThresholdLarge'),
            },
            {
              value: 40,
              name: loadTimeData.getString(
                  'autoclickMovementThresholdExtraLarge'),
            },
          ];
        },
      },

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

      /**
       * Whether the user is in kiosk mode.
       */
      isKioskModeActive_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isKioskModeActive');
        },
      },

      /**
       * Whether a setting for enabling shelf navigation buttons in tablet mode
       * should be displayed in the accessibility settings.
       */
      showShelfNavigationButtonsSettings_: {
        type: Boolean,
        computed:
            'computeShowShelfNavigationButtonsSettings_(isKioskModeActive_)',
      },

      /**
       * Whether the user is in guest mode.
       */
      isGuest_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isGuest');
        },
      },

      dictationLocaleSubtitleOverride_: {
        type: String,
        value: '',
      },

      useDictationLocaleSubtitleOverride_: {
        type: Boolean,
        value: false,
      },

      dictationLocaleMenuSubtitle_: {
        type: String,
        computed: 'computeDictationLocaleSubtitle_(' +
            'dictationLocaleOptions_, ' +
            'prefs.settings.a11y.dictation_locale.value, ' +
            'dictationLocaleSubtitleOverride_)',
      },

      dictationLocaleOptions_: {
        type: Array,
        value() {
          return [];
        },
      },

      dictationLocalesList_: {
        type: Array,
        value() {
          return [];
        },
      },

      showDictationLocaleMenu_: {
        type: Boolean,
        value: false,
      },

      dictationLearnMoreUrl_: {
        type: String,
        value: 'https://support.google.com/chromebook?p=text_dictation_m100',
      },

      /**
       * |hasKeyboard_|, |hasMouse_|, |hasPointingStick_|, and |hasTouchpad_|
       * start undefined so observers don't trigger until they have been
       * populated.
       */
      hasKeyboard_: Boolean,

      hasMouse_: Boolean,

      hasPointingStick_: Boolean,

      hasTouchpad_: Boolean,

      isAccessibilityChromeVoxPageMigrationEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'isAccessibilityChromeVoxPageMigrationEnabled');
        },
      },

      isAccessibilitySelectToSpeakPageMigrationEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'isAccessibilitySelectToSpeakPageMigrationEnabled');
        },
      },

      /**
       * Boolean indicating whether shelf navigation buttons should implicitly
       * be enabled in tablet mode - the navigation buttons are implicitly
       * enabled when spoken feedback, automatic clicks, or switch access are
       * enabled. The buttons can also be explicitly enabled by a designated
       * a11y setting.
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
       */
      shelfNavigationButtonsPref_: {
        type: Object,
        computed: 'getShelfNavigationButtonsEnabledPref_(' +
            'shelfNavigationButtonsImplicitlyEnabled_,' +
            'prefs.settings.a11y.tablet_mode_shelf_nav_buttons_enabled)',
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kChromeVox,
          Setting.kSelectToSpeak,
          Setting.kHighContrastMode,
          Setting.kFullscreenMagnifier,
          Setting.kFullscreenMagnifierMouseFollowingMode,
          Setting.kFullscreenMagnifierFocusFollowing,
          Setting.kDockedMagnifier,
          Setting.kStickyKeys,
          Setting.kOnScreenKeyboard,
          Setting.kDictation,
          Setting.kHighlightKeyboardFocus,
          Setting.kHighlightTextCaret,
          Setting.kAutoClickWhenCursorStops,
          Setting.kLargeCursor,
          Setting.kHighlightCursorWhileMoving,
          Setting.kTabletNavigationButtons,
          Setting.kMonoAudio,
          Setting.kStartupSound,
          Setting.kEnableSwitchAccess,
          Setting.kEnableCursorColor,
        ]),
      },
    };
  }

  static get observers() {
    return [
      'pointersChanged(hasMouse_, hasPointingStick_, hasTouchpad_, ' +
          'isKioskModeActive_)',
    ];
  }

  private autoClickDelayOptions_: Option[];
  private autoClickMovementThresholdOptions_: Option[];
  private cursorColorOptions_: Option[];
  private deviceBrowserProxy_: DevicePageBrowserProxy;
  private dictationLearnMoreUrl_: string;
  private dictationLocalesList_: LocaleInfo[];
  private dictationLocaleMenuSubtitle_: string;
  private dictationLocaleOptions_: LocaleInfo[];
  private dictationLocaleSubtitleOverride_: string;
  private hasKeyboard_: boolean;
  private hasMouse_: boolean;
  private hasPointingStick_: boolean;
  private hasTouchpad_: boolean;
  private isAccessibilityChromeVoxPageMigrationEnabled_: boolean;
  private isAccessibilitySelectToSpeakPageMigrationEnabled_: boolean;
  private isGuest_: boolean;
  private isKioskModeActive_: boolean;
  private manageBrowserProxy_: ManageA11ySubpageBrowserProxy;
  private route_: Route;
  private screenMagnifierMouseFollowingModePrefValues_: Record<string, number>;
  private screenMagnifierZoomOptions_: Option[];
  private shelfNavigationButtonsImplicitlyEnabled_: boolean;
  private shelfNavigationButtonsPref_:
      chrome.settingsPrivate.PrefObject<boolean>;
  private showDictationLocaleMenu_: boolean;
  private showShelfNavigationButtonsSettings_: boolean;
  private useDictationLocaleSubtitleOverride_: boolean;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route_ = routes.MANAGE_ACCESSIBILITY;

    this.manageBrowserProxy_ = ManageA11ySubpageBrowserProxyImpl.getInstance();

    this.deviceBrowserProxy_ = DevicePageBrowserProxyImpl.getInstance();

    if (!this.isKioskModeActive_) {
      this.redirectToNewA11ySettings();
    }
  }

  redirectToNewA11ySettings(): void {
    location.href = 'chrome://os-settings/osAccessibility';
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.addWebUiListener(
        'has-mouse-changed',
        (exists: boolean) => this.set('hasMouse_', exists));
    this.addWebUiListener(
        'has-pointing-stick-changed',
        (exists: boolean) => this.set('hasPointingStick_', exists));
    this.addWebUiListener(
        'has-touchpad-changed',
        (exists: boolean) => this.set('hasTouchpad_', exists));
    this.deviceBrowserProxy_.initializePointers();
    this.addWebUiListener(
        'has-hardware-keyboard',
        (hasKeyboard: boolean) => this.set('hasKeyboard_', hasKeyboard));
    this.deviceBrowserProxy_.initializeKeyboardWatcher();
  }

  override ready(): void {
    super.ready();

    this.addWebUiListener(
        'initial-data-ready',
        (startupSoundEnabled: boolean) =>
            this.onManageAllyPageReady_(startupSoundEnabled));
    this.addWebUiListener(
        'dictation-locale-menu-subtitle-changed',
        (result: string) => this.onDictationLocaleMenuSubtitleChanged_(result));
    this.addWebUiListener(
        'dictation-locales-set',
        (locales: LocaleInfo[]) => this.onDictationLocalesSet_(locales));
    this.manageBrowserProxy_.manageA11yPageReady();

    const r = routes;
    this.addFocusConfig(r.MANAGE_TTS_SETTINGS, '#ttsSubpageButton');
    this.addFocusConfig(
        r.MANAGE_SWITCH_ACCESS_SETTINGS, '#switchAccessSubpageButton');
    this.addFocusConfig(r.DISPLAY, '#displaySubpageButton');
    this.addFocusConfig(r.KEYBOARD, '#keyboardSubpageButton');
    this.addFocusConfig(r.POINTERS, '#pointerSubpageButton');
  }

  /**
   * Note: Overrides RouteOriginMixin implementation
   */
  override currentRouteChanged(newRoute: Route, prevRoute?: Route) {
    super.currentRouteChanged(newRoute, prevRoute);

    // Does not apply to this page.
    if (newRoute !== this.route_) {
      return;
    }

    this.attemptDeepLink();
  }

  private pointersChanged(
      hasMouse: boolean, hasTouchpad: boolean, hasPointingStick: boolean,
      isKioskModeActive: boolean): void {
    this.$.pointerSubpageButton.hidden =
        (!hasMouse && !hasPointingStick && !hasTouchpad) || isKioskModeActive;
  }

  /**
   * Return ChromeVox description text based on whether ChromeVox is enabled.
   */
  private getChromeVoxDescription_(enabled: boolean): string {
    return this.i18n(
        enabled ? 'chromeVoxDescriptionOn' : 'chromeVoxDescriptionOff');
  }

  /**
   * Return Fullscreen magnifier description text based on whether Fullscreen
   * magnifier is enabled.
   */
  private getScreenMagnifierDescription_(enabled: boolean): string {
    return this.i18n(
        enabled ? 'screenMagnifierDescriptionOn' :
                  'screenMagnifierDescriptionOff');
  }

  /**
   * Return Select-to-Speak description text based on:
   *    1. Whether Select-to-Speak is enabled.
   *    2. If it is enabled, whether a physical keyboard is present.
   */
  private getSelectToSpeakDescription_(enabled: boolean, hasKeyboard: boolean):
      string {
    if (!enabled) {
      return this.i18n('selectToSpeakDisabledDescription');
    }
    if (hasKeyboard) {
      return this.i18n('selectToSpeakDescription');
    }
    return this.i18n('selectToSpeakDescriptionWithoutKeyboard');
  }

  private toggleStartupSoundEnabled_(e: CustomEvent<boolean>): void {
    this.manageBrowserProxy_.setStartupSoundEnabled(e.detail);
  }

  private onManageTtsSettingsClick_(): void {
    Router.getInstance().navigateTo(routes.MANAGE_TTS_SETTINGS);
  }

  private onChromeVoxSettingsClick_(): void {
    this.manageBrowserProxy_.showChromeVoxSettings();
  }

  private onChromeVoxNewSettingsClick_(): void {
    Router.getInstance().navigateTo(routes.A11Y_CHROMEVOX);
  }

  private onChromeVoxTutorialClick_(): void {
    this.manageBrowserProxy_.showChromeVoxTutorial();
  }

  private onSelectToSpeakSettingsClick_(): void {
    this.manageBrowserProxy_.showSelectToSpeakSettings();
  }

  private onSelectToSpeakNewSettingsClick_(): void {
    Router.getInstance().navigateTo(routes.A11Y_SELECT_TO_SPEAK);
  }

  private onSwitchAccessSettingsClick_(): void {
    Router.getInstance().navigateTo(routes.MANAGE_SWITCH_ACCESS_SETTINGS);
  }

  private onDisplayClick_(): void {
    Router.getInstance().navigateTo(
        routes.DISPLAY,
        /* dynamicParams */ undefined, /* removeSearch */ true);
  }

  private onAppearanceClick_(): void {
    // Open browser appearance section in a new browser tab.
    window.open('chrome://settings/appearance');
  }

  private onKeyboardClick_(): void {
    Router.getInstance().navigateTo(
        routes.KEYBOARD,
        /* dynamicParams */ undefined, /* removeSearch */ true);
  }

  private onA11yCaretBrowsingChange_(event: Event): void {
    if ((event.target as SettingsToggleButtonElement).checked) {
      chrome.metricsPrivate.recordUserAction(
          'Accessibility.CaretBrowsing.EnableWithSettings');
    } else {
      chrome.metricsPrivate.recordUserAction(
          'Accessibility.CaretBrowsing.DisableWithSettings');
    }
  }

  private computeShowShelfNavigationButtonsSettings_(): boolean {
    return !this.isKioskModeActive_ &&
        loadTimeData.getBoolean('showTabletModeShelfNavigationButtonsSettings');
  }

  /**
   * @return Whether shelf navigation buttons should implicitly be
   *     enabled in tablet mode (due to accessibility settings different than
   *     shelf_navigation_buttons_enabled_in_tablet_mode).
   */
  private computeShelfNavigationButtonsImplicitlyEnabled_(): boolean {
    /**
     * Gets the bool pref value for the provided pref key.
     */
    const getBoolPrefValue = (key: string): boolean => {
      const pref: chrome.settingsPrivate.PrefObject<boolean>|undefined =
          this.get(key, this.prefs);
      return !!pref?.value;
    };

    return getBoolPrefValue('settings.accessibility') ||
        getBoolPrefValue('settings.a11y.autoclick') ||
        getBoolPrefValue('settings.a11y.switch_access.enabled');
  }

  /**
   * Calculates the effective value for "shelf navigation buttons enabled in
   * tablet mode" setting - if the setting is implicitly enabled (by other a11y
   * settings), this will return a stub pref value.
   */
  private getShelfNavigationButtonsEnabledPref_():
      chrome.settingsPrivate.PrefObject<boolean> {
    if (this.shelfNavigationButtonsImplicitlyEnabled_) {
      return {
        value: true,
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        key: '',
      };
    }

    return this.getPref<boolean>(
        'settings.a11y.tablet_mode_shelf_nav_buttons_enabled');
  }

  private onShelfNavigationButtonsLearnMoreClicked_(): void {
    chrome.metricsPrivate.recordUserAction(
        'Settings_A11y_ShelfNavigationButtonsLearnMoreClicked');
  }

  /**
   * Handles the <code>tablet_mode_shelf_nav_buttons_enabled</code> setting's
   * toggle changes. It updates the backing pref value, unless the setting is
   * implicitly enabled.
   */
  private updateShelfNavigationButtonsEnabledPref_(): void {
    if (this.shelfNavigationButtonsImplicitlyEnabled_) {
      return;
    }

    const enabled = this.shadowRoot!
                        .querySelector<SettingsToggleButtonElement>(
                            '#shelfNavigationButtonsEnabledControl')!.checked;

    this.setPrefValue(
        'settings.a11y.tablet_mode_shelf_nav_buttons_enabled', enabled);
    this.manageBrowserProxy_.recordSelectedShowShelfNavigationButtonValue(
        enabled);
  }

  private onA11yCursorColorChange_(): void {
    // Custom cursor color is enabled when the color is not set to black.
    const a11yCursorColorOn =
        this.getPref<number>('settings.a11y.cursor_color').value !==
        DEFAULT_BLACK_CURSOR_COLOR;
    this.setPrefValue('settings.a11y.cursor_color_enabled', a11yCursorColorOn);
  }


  private onMouseClick_(): void {
    Router.getInstance().navigateTo(
        routes.POINTERS,
        /* dynamicParams */ undefined, /* removeSearch */ true);
  }

  /**
   * Handles updating the visibility of the shelf navigation buttons setting
   * and updating whether startupSoundEnabled is checked.
   */
  private onManageAllyPageReady_(startupSoundEnabled: boolean): void {
    this.$.startupSoundEnabled.checked = startupSoundEnabled;
  }

  /**
   * Whether additional features link should be shown.
   */
  private shouldShowAdditionalFeaturesLink_(isKiosk: boolean, isGuest: boolean):
      boolean {
    return !isKiosk && !isGuest;
  }

  private onDictationLocaleMenuSubtitleChanged_(subtitle: string): void {
    this.useDictationLocaleSubtitleOverride_ = true;
    this.dictationLocaleSubtitleOverride_ = subtitle;
  }


  /**
   * Saves a list of locales and updates the UI to reflect the list.
   */
  private onDictationLocalesSet_(locales: LocaleInfo[]): void {
    this.dictationLocalesList_ = locales;
    this.onDictationLocalesChanged_();
  }

  /**
   * Converts an array of locales and their human-readable equivalents to
   * an array of menu options.
   * TODO(crbug.com/1195916): Use 'offline' to indicate to the user which
   * locales work offline with an icon in the select options.
   */
  private onDictationLocalesChanged_(): void {
    const currentLocale = this.getCurrentLocale_();
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
   */
  private computeDictationLocaleSubtitle_(): string {
    if (this.useDictationLocaleSubtitleOverride_) {
      // Only use the subtitle override once, since we still want the subtitle
      // to repsond to changes to the dictation locale.
      this.useDictationLocaleSubtitleOverride_ = false;
      return this.dictationLocaleSubtitleOverride_;
    }

    const currentLocale = this.getCurrentLocale_();
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

  private getCurrentLocale_(): string|undefined {
    // Note: b/269571551 In kiosk mode, prefs may not be initialized when this
    // settings page is rendered and runs the first computation of
    // computeDictationLocaleSubtitle_()
    if (!this.prefs) {
      return undefined;
    }

    return this.getPref<string>('settings.a11y.dictation_locale').value;
  }

  private onChangeDictationLocaleButtonClicked_(): void {
    this.showDictationLocaleMenu_ = true;
  }

  private onChangeDictationLocalesDialogClosed_(): void {
    this.showDictationLocaleMenu_ = false;
  }

  private onAdditionalFeaturesClick_(): void {
    window.open(
        'https://chrome.google.com/webstore/category/collection/3p_accessibility_extensions');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsManageA11ySubpageElement.is]: SettingsManageA11ySubpageElement;
  }
}

customElements.define(
    SettingsManageA11ySubpageElement.is, SettingsManageA11ySubpageElement);
