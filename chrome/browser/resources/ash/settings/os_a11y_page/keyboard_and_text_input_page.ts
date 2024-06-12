// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-keyboard-and-text-input-page' is the accessibility settings subpage
 * for keyboard and text input accessibility settings.
 */

import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_tooltip_icon.js';
import '../controls/settings_slider.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';
import './change_dictation_locale_dialog.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {SliderTick} from 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {cast} from '../assert_extras.js';
import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './keyboard_and_text_input_page.html.js';
import {KeyboardAndTextInputPageBrowserProxy, KeyboardAndTextInputPageBrowserProxyImpl} from './keyboard_and_text_input_page_browser_proxy.js';

interface LocaleInfo {
  name: string;
  value: string;
  worksOffline: boolean;
  installed: boolean;
  recommended: boolean;
}

const SettingsKeyboardAndTextInputPageElementBase =
    DeepLinkingMixin(RouteOriginMixin(
        PrefsMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))));

export class SettingsKeyboardAndTextInputPageElement extends
    SettingsKeyboardAndTextInputPageElementBase {
  static get is() {
    return 'settings-keyboard-and-text-input-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether the user is in kiosk mode.
       */
      isKioskModeActive_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isKioskModeActive');
        },
      },

      caretBlinkIntervalVirtualPref_: {
        type: Object,
        computed: 'computeCaretBlinkIntervalVirtualPref_(' +
            'prefs.settings.a11y.caret.blink_interval.value)',
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

      isAccessibilityCaretBlinkIntervalSettingEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'isAccessibilityCaretBlinkIntervalSettingEnabled');
        },
      },

      showDictationLocaleMenu_: {
        type: Boolean,
        value: false,
      },

      dictationLearnMoreUrl_: {
        type: String,
        value() {
          return loadTimeData.getBoolean('isKioskModeActive') ?
              '' :
              'https://support.google.com/chromebook?p=text_dictation_m100';
        },
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kCaretBlinkInterval,
          Setting.kCaretBrowsing,
          Setting.kDictation,
          Setting.kEnableSwitchAccess,
          Setting.kHighlightKeyboardFocus,
          Setting.kHighlightTextCaret,
          Setting.kOnScreenKeyboard,
          Setting.kStickyKeys,
        ]),
      },

      focusHighlightEnabledVirtualPref_: {
        type: Object,
        computed: 'computeEnabledWithConflictingFeature_(' +
            'prefs.settings.a11y.focus_highlight.value, ' +
            'prefs.settings.accessibility.value)',
      },

      stickyKeysEnabledVirtualPref_: {
        type: Object,
        computed: 'computeEnabledWithConflictingFeature_(' +
            'prefs.settings.a11y.sticky_keys_enabled.value, ' +
            'prefs.settings.accessibility.value)',
      },
    };
  }

  static get observers() {
    return [
      'updateCaretBlinkIntervalFromVirtualPref_(' +
          'caretBlinkIntervalVirtualPref_.*)',
    ];
  }

  private dictationLearnMoreUrl_: string;
  private dictationLocaleMenuSubtitle_: string;
  private dictationLocaleOptions_: LocaleInfo[];
  private dictationLocaleSubtitleOverride_: string;
  private dictationLocalesList_: LocaleInfo[];
  private isAccessibilityCaretBlinkIntervalSettingEnabled_: boolean;
  private isKioskModeActive_: boolean;
  private focusHighlightEnabledPref_:
      chrome.settingsPrivate.PrefObject<boolean>;
  private keyboardAndTextInputBrowserProxy_:
      KeyboardAndTextInputPageBrowserProxy;
  private stickyKeysEnabledVirtualPref_:
      chrome.settingsPrivate.PrefObject<boolean>;
  private showDictationLocaleMenu_: boolean;
  private useDictationLocaleSubtitleOverride_: boolean;
  private caretBlinkIntervalVirtualPref_:
      chrome.settingsPrivate.PrefObject<number>;
  private defaultCaretBlinkRateMs_: number;
  private caretBlinkIntervalOffSliderValue_ = 40;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = routes.A11Y_KEYBOARD_AND_TEXT_INPUT;

    this.keyboardAndTextInputBrowserProxy_ =
        KeyboardAndTextInputPageBrowserProxyImpl.getInstance();

    this.dictationLocaleSubtitleOverride_ = '';

    this.useDictationLocaleSubtitleOverride_ = false;

    this.defaultCaretBlinkRateMs_ =
        loadTimeData.getInteger('defaultCaretBlinkIntervalMs');
  }

  override ready(): void {
    super.ready();
    this.addWebUiListener(
        'dictation-locale-menu-subtitle-changed',
        (result: string) => this.onDictationLocaleMenuSubtitleChanged_(result));
    this.addWebUiListener(
        'dictation-locales-set',
        (locales: LocaleInfo[]) => this.onDictationLocalesSet_(locales));
    this.keyboardAndTextInputBrowserProxy_.keyboardAndTextInputPageReady();

    const r = routes;
    this.addFocusConfig(
        r.MANAGE_SWITCH_ACCESS_SETTINGS, '#switchAccessSubpageButton');
    this.addFocusConfig(r.KEYBOARD, '#keyboardSubpageButton');
  }

  /**
   * Note: Overrides RouteOriginMixin implementation
   */
  override currentRouteChanged(newRoute: Route, prevRoute?: Route): void {
    super.currentRouteChanged(newRoute, prevRoute);

    // Does not apply to this page.
    if (newRoute !== this.route) {
      return;
    }

    this.attemptDeepLink();
  }

  private onSwitchAccessSettingsClick_(): void {
    Router.getInstance().navigateTo(routes.MANAGE_SWITCH_ACCESS_SETTINGS);
  }

  private onKeyboardClick_(): void {
    Router.getInstance().navigateTo(
        routes.KEYBOARD,
        /* dynamicParams= */ undefined, /* removeSearch= */ true);
  }

  private onA11yCaretBrowsingChange_(event: Event): void {
    const targetEl = cast(event.target, SettingsToggleButtonElement);
    if (targetEl.checked) {
      chrome.metricsPrivate.recordUserAction(
          'Accessibility.CaretBrowsing.EnableWithSettings');
    } else {
      chrome.metricsPrivate.recordUserAction(
          'Accessibility.CaretBrowsing.DisableWithSettings');
    }
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
   * TODO(crbug.com/40176223): Use 'offline' to indicate to the user which
   * locales work offline with an icon in the select options.
   */
  private onDictationLocalesChanged_(): void {
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
   */
  private computeDictationLocaleSubtitle_(): string {
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

  private onChangeDictationLocaleButtonClicked_(): void {
    this.showDictationLocaleMenu_ = true;
  }

  private onChangeDictationLocalesDialogClosed_(): void {
    this.showDictationLocaleMenu_ = false;
  }

  private computeEnabledWithConflictingFeature_(
      prefValue: boolean, conflictingPrefValue: boolean):
      chrome.settingsPrivate.PrefObject<boolean> {
    return {
      value: !conflictingPrefValue && prefValue,
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      key: '',
    };
  }

  private computeCaretBlinkIntervalVirtualPref_():
      chrome.settingsPrivate.PrefObject<number> {
    if (!this.isAccessibilityCaretBlinkIntervalSettingEnabled_ || !this.prefs) {
      return {
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: this.defaultCaretBlinkRateMs_,
        key: 'caret_blink_interval_virtual_pref',
      };
    }
    const blinkIntervalMs =
        this.getPref<number>('settings.a11y.caret.blink_interval').value;
    let value = this.caretBlinkIntervalOffSliderValue_;
    if (blinkIntervalMs > 0) {
      value = Math.round(this.defaultCaretBlinkRateMs_ / blinkIntervalMs * 100);
    }
    return {
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value,
      key: 'caret_blink_interval_virtual_pref',
    };
  }

  private updateCaretBlinkIntervalFromVirtualPref_(): void {
    if (!this.isAccessibilityCaretBlinkIntervalSettingEnabled_) {
      return;
    }
    const percentage = this.caretBlinkIntervalVirtualPref_.value;
    // Default: do not blink.
    let delayMs = 0;
    if (percentage > this.caretBlinkIntervalOffSliderValue_) {
      delayMs = Math.round(this.defaultCaretBlinkRateMs_ / (percentage / 100));
    }
    this.setPrefValue('settings.a11y.caret.blink_interval', delayMs);
  }

  private computeCaretBlinkIntervalTicks_(): SliderTick[] {
    const ticks = [
      {
        value: this.caretBlinkIntervalOffSliderValue_,
        ariaValue: 0,
        label: this.i18n('caretBlinkIntervalOff'),
      },
    ];
    for (let i = this.caretBlinkIntervalOffSliderValue_ + 10; i <= 150;
         i += 10) {
      const label = i === 100 ? this.i18n('defaultPercentage', i) :
                                this.i18n('percentage', i);
      ticks.push({
        value: i,
        ariaValue: i,
        label,
      });
    }
    return ticks;
  }

  private updateFocusHighlightEnabledVirtualPref_(): void {
    // Focus highlight is automatically disabled when ChromeVox is
    // enabled, although the underlying pref is unchanged (allows
    // for state restore if ChromeVox is later disabled).)
    // Reflect the fact focus highlight isn't running by showing
    // the toggle as off.
    if (this.getPref<boolean>('settings.accessibility').value) {
      return;
    }
    this.setPrefValue(
        'settings.a11y.focus_highlight',
        !this.getPref<boolean>('settings.a11y.focus_highlight').value);
  }

  private updateStickyKeysEnabledVirtualPref_(): void {
    // Sticky keys is automatically disabled when ChromeVox is
    // enabled, although the underlying pref is unchanged (allows
    // for state restore if ChromeVox is later disabled).)
    // Reflect the fact sticky keys isn't running by showing
    // the toggle as off.
    if (this.getPref<boolean>('settings.accessibility').value) {
      return;
    }
    this.setPrefValue(
        'settings.a11y.sticky_keys_enabled',
        !this.getPref<boolean>('settings.a11y.sticky_keys_enabled').value);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-keyboard-and-text-input-page':
        SettingsKeyboardAndTextInputPageElement;
  }
}

customElements.define(
    SettingsKeyboardAndTextInputPageElement.is,
    SettingsKeyboardAndTextInputPageElement);
