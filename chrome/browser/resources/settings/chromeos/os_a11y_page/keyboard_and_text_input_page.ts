// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-keyboard-and-text-input-page' is the accessibility settings subpage
 * for keyboard and text input accessibility settings.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../../controls/settings_slider.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared.css.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin, WebUiListenerMixinInterface} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {PrefsMixin, PrefsMixinInterface} from '../../prefs/prefs_mixin.js';
import {cast} from '../assert_extras.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteOriginMixin, RouteOriginMixinInterface} from '../route_origin_mixin.js';
import {Route, Router} from '../router.js';

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
    mixinBehaviors(
        [
          DeepLinkingBehavior,
        ],
        RouteOriginMixin(
            PrefsMixin(WebUiListenerMixin(I18nMixin(PolymerElement))))) as {
      new (): PolymerElement & I18nMixinInterface &
          WebUiListenerMixinInterface & PrefsMixinInterface &
          RouteOriginMixinInterface & DeepLinkingBehaviorInterface,
    };

class SettingsKeyboardAndTextInputPageElement extends
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
       * Used by DeepLinkingBehavior to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kStickyKeys,
          Setting.kOnScreenKeyboard,
          Setting.kDictation,
          Setting.kHighlightKeyboardFocus,
          Setting.kHighlightTextCaret,
          Setting.kEnableSwitchAccess,
        ]),
      },
    };
  }

  private dictationLearnMoreUrl_: string;
  private dictationLocaleMenuSubtitle_: string;
  private dictationLocaleOptions_: LocaleInfo[];
  private dictationLocaleSubtitleOverride_: string;
  private dictationLocalesList_: LocaleInfo[];
  private isKioskModeActive_: boolean;
  private keyboardAndTextInputBrowserProxy_:
      KeyboardAndTextInputPageBrowserProxy;
  private route_: Route;
  private showDictationLocaleMenu_: boolean;
  private useDictationLocaleSubtitleOverride_: boolean;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route_ = routes.A11Y_KEYBOARD_AND_TEXT_INPUT;

    this.keyboardAndTextInputBrowserProxy_ =
        KeyboardAndTextInputPageBrowserProxyImpl.getInstance();

    this.dictationLocaleSubtitleOverride_ = '';

    this.useDictationLocaleSubtitleOverride_ = false;
  }

  override ready() {
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
  override currentRouteChanged(newRoute: Route, prevRoute?: Route) {
    super.currentRouteChanged(newRoute, prevRoute);

    // Does not apply to this page.
    if (newRoute !== routes.A11Y_KEYBOARD_AND_TEXT_INPUT) {
      return;
    }

    this.attemptDeepLink();
  }

  private onSwitchAccessSettingsTap_(): void {
    Router.getInstance().navigateTo(routes.MANAGE_SWITCH_ACCESS_SETTINGS);
  }

  private onKeyboardTap_(): void {
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
   * TODO(crbug.com/1195916): Use 'offline' to indicate to the user which
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
