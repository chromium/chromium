// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {assert, assertInstanceof} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {DomRepeat} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {BrowserProxy} from './browser_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import {LanguageBrowserProxyImpl} from './language_browser_proxy.js';
import type {LanguageBrowserProxy} from './language_browser_proxy.js';
import {getTemplate} from './translate_button.html.js';

// The language codes that are supported to be translated by the server.
const SUPPORTED_TRANSLATION_LANGUAGES = new Set([
  'af',  'sq',  'am',    'ar',    'hy', 'az', 'eu', 'be', 'bn', 'bs', 'bg',
  'ca',  'ceb', 'zh-CN', 'zh-TW', 'co', 'hr', 'cs', 'da', 'nl', 'en', 'eo',
  'et',  'fi',  'fr',    'fy',    'gl', 'ka', 'de', 'el', 'gu', 'ht', 'ha',
  'haw', 'hi',  'hmn',   'hu',    'is', 'ig', 'id', 'ga', 'it', 'iw', 'ja',
  'jv',  'kn',  'kk',    'km',    'rw', 'ko', 'ku', 'ky', 'lo', 'la', 'lv',
  'lt',  'lb',  'mk',    'mg',    'ms', 'ml', 'mt', 'mi', 'mr', 'mn', 'my',
  'ne',  'no',  'ny',    'or',    'ps', 'fa', 'pl', 'pt', 'pa', 'ro', 'ru',
  'sm',  'gd',  'sr',    'st',    'sn', 'sd', 'si', 'sk', 'sl', 'so', 'es',
  'su',  'sw',  'sv',    'tl',    'tg', 'ta', 'tt', 'te', 'th', 'tr', 'tk',
  'uk',  'ur',  'ug',    'uz',    'vi', 'cy', 'xh', 'yi', 'yo', 'zu',
]);

export interface TranslateButtonElement {
  $: {
    languagePicker: HTMLDivElement,
    sourceAutoDetectButton: CrButtonElement,
    sourceLanguageButton: CrButtonElement,
    sourceLanguagePickerContainer: DomRepeat,
    sourceLanguagePickerMenu: HTMLDivElement,
    targetLanguageButton: CrButtonElement,
    targetLanguagePickerContainer: DomRepeat,
    targetLanguagePickerMenu: HTMLDivElement,
    translateButton: CrButtonElement,
  };
}

export class TranslateButtonElement extends PolymerElement {
  static get is() {
    return 'translate-button';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isTranslateModeEnabled: {
        type: Boolean,
        reflectToAttribute: true,
      },
      shouldShowStarsIcon: {
        type: Boolean,
        computed: 'computeShouldShowStarsIcon(sourceLanguage)',
        reflectToAttribute: true,
      },
      sourceLanguage: Object,
      sourceLanguageMenuVisible: {
        type: Boolean,
        reflectToAttribute: true,
      },
      targetLanguage: Object,
      targetLanguageMenuVisible: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  // Whether the translate mode on the lens overlay has been enabled.
  private isTranslateModeEnabled: boolean = false;
  // Whether the stars icon is visible on the source language button.
  private shouldShowStarsIcon: boolean;
  // The currently selected source language to translate to. If null, we should
  // auto detect the language.
  private sourceLanguage: chrome.languageSettingsPrivate.Language|null = null;
  // The currently selected target language to translate to.
  private targetLanguage: chrome.languageSettingsPrivate.Language;
  // Whether the source language menu picker is visible.
  private sourceLanguageMenuVisible: boolean = false;
  // Whether the target language menu picker is visible.
  private targetLanguageMenuVisible: boolean = false;
  // The list of target languages provided by the chrome API.
  private translateLanguageList: chrome.languageSettingsPrivate.Language[];
  // A browser proxy for communicating with the C++ Lens overlay controller.
  private browserProxy: BrowserProxy = BrowserProxyImpl.getInstance();
  // A browser proxy for fetching the language settings from the Chrome API.
  private languageBrowserProxy: LanguageBrowserProxy =
      LanguageBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.languageBrowserProxy.getLanguageList().then(
        this.onLanguageListRetrieved.bind(this));
  }

  private onLanguageListRetrieved(
      languageList: chrome.languageSettingsPrivate.Language[]) {
    this.translateLanguageList = languageList.filter((language) => {
      return SUPPORTED_TRANSLATION_LANGUAGES.has(language.code);
    });

    // After receiving the language list, get the default translate target
    // language. This needs to happen after fetching the language list so we can
    //  use the list to fetch the language's display name.
    this.languageBrowserProxy.getTranslateTargetLanguage().then(
        this.onTargetLanguageRetrieved.bind(this));
  }

  private onTargetLanguageRetrieved(languageCode: string) {
    const defaultLanguage = this.translateLanguageList.find(
        language => language.code === languageCode);
    assert(defaultLanguage);
    this.targetLanguage = defaultLanguage;
  }

  private onAutoDetectMenuItemClick() {
    this.sourceLanguage = null;
    this.hideLanguagePickerMenus();
  }

  private onSourceLanguageButtonClick() {
    this.sourceLanguageMenuVisible = !this.sourceLanguageMenuVisible;
    this.targetLanguageMenuVisible = false;
  }

  private onTargetLanguageButtonClick() {
    this.targetLanguageMenuVisible = !this.targetLanguageMenuVisible;
    this.sourceLanguageMenuVisible = false;
  }

  private onSourceLanguageMenuItemClick(event: PointerEvent) {
    assertInstanceof(event.target, HTMLElement);
    const newSourceLanguage =
        this.$.sourceLanguagePickerContainer.itemForElement(event.target);
    this.sourceLanguage = newSourceLanguage;
    this.hideLanguagePickerMenus();
  }

  private onTargetLanguageMenuItemClick(event: PointerEvent) {
    assertInstanceof(event.target, HTMLElement);
    const newTargetLanguage =
        this.$.targetLanguagePickerContainer.itemForElement(event.target);
    this.targetLanguage = newTargetLanguage;
    this.hideLanguagePickerMenus();
  }

  private onTranslateButtonClick() {
    // Toggle translate mode on button click.
    this.isTranslateModeEnabled = !this.isTranslateModeEnabled;

    if (this.isTranslateModeEnabled) {
      this.browserProxy.handler.issueTranslateFullPageRequest(
          this.sourceLanguage ? this.sourceLanguage.code : 'auto',
          this.targetLanguage.code);
    }
  }

  private hideLanguagePickerMenus() {
    this.targetLanguageMenuVisible = false;
    this.sourceLanguageMenuVisible = false;
  }

  private getSourceLanguageDisplayName(): string {
    if (this.sourceLanguage) {
      return this.sourceLanguage.displayName;
    }

    return loadTimeData.getString('autoDetect');
  }

  private computeShouldShowStarsIcon(): boolean {
    return this.sourceLanguage === null;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'translate-button': TranslateButtonElement;
  }
}

customElements.define(TranslateButtonElement.is, TranslateButtonElement);
