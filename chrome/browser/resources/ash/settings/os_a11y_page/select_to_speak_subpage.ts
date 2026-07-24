// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-select-to-speak-subpage' is the accessibility settings subpage for
 * Select-to-speak settings.
 */

import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../settings_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import '../controls/v2/settings_dropdown_row.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import type {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';
import type {DropdownOptionList} from '../controls/v2/settings_dropdown_v2.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import type {LanguagesBrowserProxy} from '../os_languages_page/languages_browser_proxy.js';
import {LanguagesBrowserProxyImpl} from '../os_languages_page/languages_browser_proxy.js';
import type {Route} from '../router.js';
import {Router, routes} from '../router.js';

import {getTemplate} from './select_to_speak_subpage.html.js';
import type {SelectToSpeakSubpageBrowserProxy} from './select_to_speak_subpage_browser_proxy.js';
import {SelectToSpeakSubpageBrowserProxyImpl} from './select_to_speak_subpage_browser_proxy.js';

/**
 * Constant used as the value for a menu option representing the current device
 * language.
 */
const USE_DEVICE_LANGUAGE = 'select_to_speak_device_language';

/**
 * Constant representing the system TTS voice.
 */
const SYSTEM_VOICE = 'select_to_speak_system_voice';

/**
 * Extension ID of the enhanced network TTS voices extension.
 */
const ENHANCED_TTS_EXTENSION_ID = 'jacnkoglebceckolkoapelihnglgaicd';

/**
 * Subset of TtsEventType enum from:
 *   chromium/src/content/public/browser/tts_utterance.h
 * String conversion can be found in:
 *   chrome/browser/speech/extension_api/tts_extension_api.cc
 */
enum EventType {
  START = 'start',
  END = 'end',
  WORD = 'word',
  CANCELLED = 'cancelled',
}

export interface HandlerVoice {
  eventTypes: EventType[];
  extensionId: string;
  lang: string;
  voiceName: string;
  displayName?: string;
  displayLanguage?: string;
  displayLanguageAndCountry?: string;
  languageCode?: string;
}

export interface SettingsSelectToSpeakSubpageElement {
  $: {};
}

const SettingsSelectToSpeakSubpageElementBase =
    DeepLinkingMixin(RouteOriginMixin(
        PrefsMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))));

export class SettingsSelectToSpeakSubpageElement extends
    SettingsSelectToSpeakSubpageElementBase {
  static get is() {
    return 'settings-select-to-speak-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether a voice preview is currently speaking.
       */
      isPreviewing_: {
        type: Boolean,
        value: false,
      },

      voicePreviewText_: {
        type: String,
        value: '',
      },

      /**
       * The language sort dropdown state as a fake preference object (so we can
       * use <settings-dropdown-menu> without overriding with custom handlers)
       */
      languageFilterVirtualPref_: {
        type: Object,
        observer: 'languageChanged_',
        notify: true,
        value(): chrome.settingsPrivate.PrefObject {
          return {
            key: 'fakeLanguagePref',
            type: chrome.settingsPrivate.PrefType.STRING,
            value: USE_DEVICE_LANGUAGE,
          };
        },
      },

      /**
       * List of options for the languages menu.
       */
      languagesMenuOptions_: {
        type: Array,
        value: [],
      },

      /**
       * List of options for the local voices menu.
       */
      localVoicesMenuOptions_: {
        type: Array,
        value: [],
      },

      /**
       * List of options for the text size drop-down menu.
       */
      highlightColorOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: '#5e9bff',
              name: loadTimeData.getString(
                  'selectToSpeakOptionsHighlightColorBlue'),
            },
            {
              value: '#ffa13d',
              name: loadTimeData.getString(
                  'selectToSpeakOptionsHighlightColorOrange'),
            },
            {
              value: '#eeff41',
              name: loadTimeData.getString(
                  'selectToSpeakOptionsHighlightColorYellow'),
            },
            {
              value: '#64dd17',
              name: loadTimeData.getString(
                  'selectToSpeakOptionsHighlightColorGreen'),
            },
            {
              value: '#ff4081',
              name: loadTimeData.getString(
                  'selectToSpeakOptionsHighlightColorPink'),
            },
          ];
        },
      },

      selectToSpeakLearnMoreUrl_: {
        type: String,
        value() {
          return loadTimeData.getBoolean('isKioskModeActive') ?
              '' :
              loadTimeData.getString('selectToSpeakLearnMoreUrl');
        },
      },
    };
  }

  static get observers() {
    return [
      'onHighlightColorChanged_(' +
          'prefs.settings.a11y.select_to_speak_highlight_color.value)',
      'languageChanged_(languageFilterVirtualPref_.*)',
    ];
  }

  // DeepLinkingMixin override
  override supportedSettingIds = new Set<Setting>([
    Setting.kSelectToSpeakWordHighlight,
    Setting.kSelectToSpeakBackgroundShading,
    Setting.kSelectToSpeakNavigationControls,
  ]);

  private langBrowserProxy_: LanguagesBrowserProxy;
  declare private readonly highlightColorOptions_: DropdownMenuOptionList;
  declare private isPreviewing_: boolean;
  declare private languageFilterVirtualPref_:
      chrome.settingsPrivate.PrefObject<string>;
  declare private languagesMenuOptions_: DropdownMenuOptionList;
  declare private localVoicesMenuOptions_: DropdownOptionList;
  declare private voicePreviewText_: string;
  declare private selectToSpeakLearnMoreUrl_: string;
  private appLocale_ = '';
  private selectToSpeakBrowserProxy_: SelectToSpeakSubpageBrowserProxy;
  private voices_: HandlerVoice[] = [];

  constructor() {
    super();

    this.selectToSpeakBrowserProxy_ =
        SelectToSpeakSubpageBrowserProxyImpl.getInstance();
    this.langBrowserProxy_ = LanguagesBrowserProxyImpl.getInstance();

    /** RouteOriginMixin override */
    this.route = routes.A11Y_SELECT_TO_SPEAK;
  }

  override ready(): void {
    super.ready();

    // Populate the voice preview text input with a sample message.
    // Users can change this to their own value later.
    this.voicePreviewText_ = this.i18n('textToSpeechPreviewInput');
    this.addWebUiListener(
        'all-sts-voice-data-updated',
        (voices: HandlerVoice[]) => this.updateVoices_(voices));
    this.addWebUiListener(
        'app-locale-updated',
        (appLocale: string) => this.updateAppLocale_(appLocale));
    this.addWebUiListener(
        'tts-preview-state-changed',
        (isSpeaking: boolean) => this.onTtsPreviewStateChanged_(isSpeaking));
    this.selectToSpeakBrowserProxy_.getAllTtsVoiceData();
    this.selectToSpeakBrowserProxy_.getAppLocale();
    this.selectToSpeakBrowserProxy_.refreshTtsVoices();
  }

  /**
   * Note: Overrides RouteOriginMixin implementation.
   */
  override currentRouteChanged(newRoute: Route, prevRoute?: Route): void {
    super.currentRouteChanged(newRoute, prevRoute);

    // Does not apply to this page.
    if (newRoute !== this.route) {
      return;
    }

    this.attemptDeepLink();
  }

  private onHighlightColorChanged_(color: string): void {
    this.shadowRoot!.getElementById('lightHighlight')!.style.background = color;
    this.shadowRoot!.getElementById('darkHighlight')!.style.background = color;
  }

  /**
   * Called when the TTS voice preview state changes between speaking and not
   * speaking.
   */
  private onTtsPreviewStateChanged_(isSpeaking: boolean): void {
    this.isPreviewing_ = isSpeaking;
  }

  /**
   * Returns true if voices are loaded and preview is not currently speaking and
   * there is text to preview.
   */
  private enablePreviewButton_(
      voiceOptions: DropdownMenuOptionList, isPreviewing: boolean,
      previewText: string): boolean {
    const nonWhitespaceRe = /\S+/;
    const hasPreviewText = nonWhitespaceRe.exec(previewText) !== null;
    return voiceOptions.length > 0 && !isPreviewing && hasPreviewText;
  }

  /**
   * Returns the voice name and extension matching the current primary voice
   * pref. If the primary voice pref is set to the system voice, then return
   * an empty name and extension, to tell the TTS handler to use the default
   * system voice.
   */
  private getVoiceNameAndExtension_(): {name: string, extension: string} {
    const name =
        this.getPref<string>('settings.a11y.select_to_speak_voice_name').value;
    if (name === SYSTEM_VOICE) {
      return {
        name: '',
        extension: '',
      };
    }

    const extension =
        this.voices_.find(({voiceName}) => voiceName === name)!.extensionId;
    return {name, extension};
  }

  private onVoicePreviewClick_(): void {
    this.selectToSpeakBrowserProxy_.previewTtsVoice(
        this.voicePreviewText_,
        JSON.stringify(this.getVoiceNameAndExtension_()));
  }

  private languageChanged_(): void {
    this.populateVoicesAndLanguages_();
  }

  /**
   * Updates the lists of all voices and the UI to use in display.
   */
  private updateVoices_(voices: HandlerVoice[]): void {
    this.voices_ = voices;
    this.populateVoicesAndLanguages_();
  }

  /**
   * Updates the app locale and repopulates voices and languages.
   */
  private updateAppLocale_(appLocale: string): void {
    this.appLocale_ = appLocale.toLowerCase();
    this.populateVoicesAndLanguages_();
  }

  /**
   * Populate select elements corresponding to local and network voices with a
   * list of corresponding TTS voices, and select element corresponding to
   * language with a list of languages covered by the available voices.
   * @private
   */
  private populateVoicesAndLanguages_(): void {
    let lang = this.languageFilterVirtualPref_.value || USE_DEVICE_LANGUAGE;
    if (lang === USE_DEVICE_LANGUAGE) {
      lang = this.getLanguageShortCode_(this.appLocale_);
    }

    const languagesMenuOptions = [{
      value: USE_DEVICE_LANGUAGE,
      name: this.i18n('selectToSpeakOptionsDeviceLanguage'),
    }];

    const localVoicesMenuOptions = [{
      value: SYSTEM_VOICE,
      label: this.i18n('selectToSpeakOptionsSystemVoice'),
    }];

    // Group voices by language, and languages by language family.
    this.groupAndAddLanguagesAndVoices_(
        this.voices_, lang, languagesMenuOptions, localVoicesMenuOptions);

    // Update the dropdowns on the page.
    this.languagesMenuOptions_ = languagesMenuOptions;
    this.localVoicesMenuOptions_ = localVoicesMenuOptions;
  }

  /**
   * Group and sort available voices by language, and add languages and local
   * voices to their respective select elements.
   * TODO(crbug.com/1234115): Add unit tests for this method.
   */
  private groupAndAddLanguagesAndVoices_(
      voices: HandlerVoice[], preferredLang: string,
      languageOptions: DropdownMenuOptionList,
      localOptions: DropdownOptionList): void {
    // Group voices by language.
    const languageDisplayNames = new Map();
    const localVoices = new Map();

    voices.forEach(voice => {
      if (!this.isVoiceUsable_(voice) ||
          voice.extensionId === ENHANCED_TTS_EXTENSION_ID) {
        return;
      }
      // Only show language names based on base language code.
      const languageCode = this.getLanguageShortCode_(voice.lang || '');
      const displayName = voice.displayLanguage;
      if (!displayName) {
        return;
      }
      languageDisplayNames.set(languageCode, displayName);
      voice.displayName = voice.voiceName;
      this.addVoiceToMapForLanguage_(voice, localVoices, languageCode);
    });

    this.populateLanguages_(languageDisplayNames, languageOptions);

    // Sort voices by language, with the preferred language on top.
    const voiceLanguagesList = Array.from(languageDisplayNames.keys());
    voiceLanguagesList.sort(
        (lang1, lang2) => (Number(lang2 === preferredLang) -
                           Number(lang1 === preferredLang)) ||
            lang1.localeCompare(lang2));

    // Populate local select.
    voiceLanguagesList.forEach(voiceLang => {
      this.appendVoicesToOptions_(localOptions, localVoices.get(voiceLang));
    });
  }

  /**
   * Populate language select element with language display names.
   * |languageDisplayNames| is a Map of language code (e.g. en) to display name
   * (e.g. English).
   */
  private populateLanguages_(
      languageDisplayNames: Map<string, string>,
      languageOptions: DropdownMenuOptionList): void {
    const supportedLanguagesList = Array.from(languageDisplayNames.keys());
    supportedLanguagesList.sort(
        (lang1, lang2) => languageDisplayNames.get(lang1)!.localeCompare(
            languageDisplayNames.get(lang2)!));
    supportedLanguagesList.forEach(language => {
      languageOptions.push(
          {value: language, name: languageDisplayNames.get(language)!});
    });
  }

  /**
   * Checks if a voice has the properties and events needed for Select-to-speak.
   */
  private isVoiceUsable_(voice: HandlerVoice): boolean {
    if (!voice.voiceName || !voice.lang) {
      return false;
    }
    if (!voice.eventTypes.includes(EventType.START) ||
        !voice.eventTypes.includes(EventType.END) ||
        !voice.eventTypes.includes(EventType.WORD) ||
        !voice.eventTypes.includes(EventType.CANCELLED)) {
      // Required event types for Select-to-Speak.
      return false;
    }
    return true;
  }

  /**
   * Returns the ISO 639 code (e.g. en or yue) for the given language code (e.g.
   * en-us).
   */
  private getLanguageShortCode_(lang: string): string {
    return lang.trim().split(/-|_/)[0];
  }

  /**
   * Add options corresponding to the given list of voices to a select element.
   */
  private appendVoicesToOptions_(
      options: DropdownOptionList, voiceList: HandlerVoice[]): void {
    if (!voiceList) {
      return;
    }
    if (voiceList.length > 1) {
      voiceList.sort((a, b) => a.displayName!.localeCompare(b.displayName!));
    }

    voiceList.forEach(voice => options.push({
      value: voice.voiceName,
      label: voice.displayName!,
    }));
  }

  /**
   * Adds a voice to the map entry corresponding to the given language.
   */
  private addVoiceToMapForLanguage_(
      voice: HandlerVoice, map: Map<string, HandlerVoice[]>,
      lang: string): void {
    voice.languageCode = lang;
    if (map.has(lang)) {
      map.get(lang)!.push(voice);
    } else {
      map.set(lang, [voice]);
    }
  }

  private onTextToSpeechSettingsClick_(): void {
    Router.getInstance().navigateTo(
        routes.MANAGE_TTS_SETTINGS,
        /* dynamicParams= */ undefined, /* removeSearch= */ true);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-select-to-speak-subpage': SettingsSelectToSpeakSubpageElement;
  }
}

customElements.define(
    SettingsSelectToSpeakSubpageElement.is,
    SettingsSelectToSpeakSubpageElement);
