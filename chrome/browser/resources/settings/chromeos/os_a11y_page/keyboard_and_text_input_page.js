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

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/cr_elements/web_ui_listener_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';
import {RouteOriginBehavior, RouteOriginBehaviorImpl, RouteOriginBehaviorInterface} from '../route_origin_behavior.js';

import {KeyboardAndTextInputPageBrowserProxy, KeyboardAndTextInputPageBrowserProxyImpl} from './keyboard_and_text_input_page_browser_proxy.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {RouteOriginBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const SettingsKeyboardAndTextInputPageElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      I18nBehavior,
      RouteObserverBehavior,
      RouteOriginBehavior,
      WebUIListenerBehavior,
    ],
    PolymerElement);

/**
 * @typedef {{
 * name: string,
 * value: string,
 * worksOffline: boolean,
 * installed: boolean,
 * recommended: boolean
 * }}
 */
let LocaleInfo;

/** @polymer */
class SettingsKeyboardAndTextInputPageElement extends
    SettingsKeyboardAndTextInputPageElementBase {
  static get is() {
    return 'settings-keyboard-and-text-input-page';
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
       * Whether the user is in kiosk mode.
       * @protected
       */
      isKioskModeActive_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isKioskModeActive');
        },
      },

      /** @protected */
      dictationLocaleMenuSubtitle_: {
        type: String,
        computed: 'computeDictationLocaleSubtitle_(' +
            'dictationLocaleOptions_, ' +
            'prefs.settings.a11y.dictation_locale.value, ' +
            'dictationLocaleSubtitleOverride_)',
      },

      /**
       * @type {!Array<!LocaleInfo>}
       * @protected
       */
      dictationLocaleOptions_: {
        type: Array,
        value() {
          return [];
        },
      },

      /** @protected */
      dictationLocalesList_: {
        type: Array,
        value() {
          return [];
        },
      },

      /** @protected */
      showDictationLocaleMenu_: {
        type: Boolean,
        value: false,
      },

      /** @protected */
      dictationLearnMoreUrl_: {
        type: String,
        value: 'https://support.google.com/chromebook?p=text_dictation_m100',
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
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

  /** @override */
  constructor() {
    super();

    /** RouteOriginBehavior override */
    this.route_ = routes.A11Y_KEYBOARD_AND_TEXT_INPUT;

    /** @private {!KeyboardAndTextInputPageBrowserProxy} */
    this.keyboardAndTextInputBrowserProxy_ =
        KeyboardAndTextInputPageBrowserProxyImpl.getInstance();

    /** @private {string} */
    this.dictationLocaleSubtitleOverride_ = '';

    /** @private {boolean} */
    this.useDictationLocaleSubtitleOverride_ = false;
  }

  /** @override */
  ready() {
    super.ready();
    this.addWebUIListener(
        'dictation-locale-menu-subtitle-changed',
        (result) => this.onDictationLocaleMenuSubtitleChanged_(result));
    this.addWebUIListener(
        'dictation-locales-set',
        (locales) => this.onDictationLocalesSet_(locales));
    this.keyboardAndTextInputBrowserProxy_.keyboardAndTextInputPageReady();

    const r = routes;
    this.addFocusConfig(
        r.MANAGE_SWITCH_ACCESS_SETTINGS, '#switchAccessSubpageButton');
    this.addFocusConfig(r.KEYBOARD, '#keyboardSubpageButton');
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
    if (newRoute !== routes.A11Y_KEYBOARD_AND_TEXT_INPUT) {
      return;
    }

    this.attemptDeepLink();
  }

  /** @private */
  onSwitchAccessSettingsTap_() {
    Router.getInstance().navigateTo(routes.MANAGE_SWITCH_ACCESS_SETTINGS);
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
   * @param {string} subtitle
   * @private
   */
  onDictationLocaleMenuSubtitleChanged_(subtitle) {
    this.useDictationLocaleSubtitleOverride_ = true;
    this.dictationLocaleSubtitleOverride_ = subtitle;
  }


  /**
   * Saves a list of locales and updates the UI to reflect the list.
   * @param {!Array<!LocaleInfo>} locales
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
    SettingsKeyboardAndTextInputPageElement.is,
    SettingsKeyboardAndTextInputPageElement);
