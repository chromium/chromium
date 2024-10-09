// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons_lit.html.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assert, assertInstanceof} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {DomRepeat} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {afterNextRender, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {BrowserProxy} from './browser_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import {LanguageBrowserProxyImpl} from './language_browser_proxy.js';
import type {LanguageBrowserProxy} from './language_browser_proxy.js';
import {UserAction} from './lens.mojom-webui.js';
import {INVOCATION_SOURCE} from './lens_overlay_app.js';
import {recordLensOverlayInteraction} from './metrics_utils.js';
import {focusShimmerOnRegion, ShimmerControlRequester, unfocusShimmer} from './selection_utils.js';
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

export interface TranslateState {
  translateModeEnabled: boolean;
  targetLanguage: string;
  shouldUnselectWords: boolean;
}

export interface TranslateButtonElement {
  $: {
    menuDetectedLanguage: HTMLDivElement,
    languagePicker: HTMLDivElement,
    sourceAutoDetectButton: CrButtonElement,
    sourceLanguageButton: CrButtonElement,
    sourceLanguagePickerBackButton: CrIconButtonElement,
    sourceLanguagePickerContainer: DomRepeat,
    sourceLanguagePickerMenu: HTMLDivElement,
    targetLanguageButton: CrButtonElement,
    targetLanguagePickerBackButton: CrIconButtonElement,
    targetLanguagePickerContainer: DomRepeat,
    targetLanguagePickerMenu: HTMLDivElement,
    translateDisableButton: CrButtonElement,
    translateEnableButton: CrButtonElement,
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
      contentLanguage: {
        type: String,
        reflectToAttribute: true,
      },
      isTranslateModeEnabled: {
        type: Boolean,
        reflectToAttribute: true,
      },
      languagePickerButtonsVisible: {
        type: Boolean,
        computed: `computeLanguagePickerButtonsVisible(
              isTranslateModeEnabled, sourceLanguageMenuVisible,
              targetLanguageMenuVisible)`,
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

  private eventTracker_: EventTracker = new EventTracker();
  // Whether the translate mode on the lens overlay has been enabled.
  private isTranslateModeEnabled: boolean = false;
  // Whether the language picker buttons are currently visible.
  private languagePickerButtonsVisible: boolean;
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
  // The content language code received from the lext layer.
  private contentLanguage: string = '';
  // A browser proxy for communicating with the C++ Lens overlay controller.
  private browserProxy: BrowserProxy = BrowserProxyImpl.getInstance();
  // A browser proxy for fetching the language settings from the Chrome API.
  private languageBrowserProxy: LanguageBrowserProxy =
      LanguageBrowserProxyImpl.getInstance();
  // An array for keeping track of the events the translate button listens to
  // from the browser proxy.
  private listenerIds: number[];

  override connectedCallback() {
    super.connectedCallback();
    this.languageBrowserProxy.getLanguageList().then(
        this.onLanguageListRetrieved.bind(this));
    this.eventTracker_.add(
        document, 'received-content-language', (e: CustomEvent) => {
          // Lens sends 'zh' and 'zh-Hant', which need to be converted to
          // 'zh-CN' and 'zh-TW' to match the language codes used by
          // chrome.languageSettingsPrivate.
          if (e.detail.contentLanguage === 'zh') {
            this.contentLanguage = 'zh-CN';
          } else if (e.detail.contentLanguage === 'zh-Hant') {
            this.contentLanguage = 'zh-TW';
          } else {
            this.contentLanguage = e.detail.contentLanguage;
          }
        });
    // Set up listener to listen to events from C++.
    this.listenerIds = [
      this.browserProxy.callbackRouter.setTranslateMode.addListener(
          this.setTranslateMode.bind(this)),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds.forEach(
        id => assert(this.browserProxy.callbackRouter.removeListener(id)));
    this.listenerIds = [];
    this.eventTracker_.removeAll();
  }

  getTranslateEnableButton(): CrButtonElement {
    return this.$.translateEnableButton;
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

    // If the target language is set to one supported by Lens, then we set it
    // and are done.
    if (defaultLanguage) {
      this.targetLanguage = defaultLanguage;
      return;
    }

    // Otherwise, we default to the first language in the list.
    this.targetLanguage = this.translateLanguageList[0];
  }

  private onAutoDetectMenuItemClick() {
    this.sourceLanguage = null;
    this.hideLanguagePickerMenus();
    this.maybeIssueTranslateRequest();
    recordLensOverlayInteraction(
        INVOCATION_SOURCE, UserAction.kTranslateSourceLanguageChanged);
  }

  private onSourceLanguageButtonClick() {
    this.sourceLanguageMenuVisible = !this.sourceLanguageMenuVisible;
    this.targetLanguageMenuVisible = false;
    // We need to wait for the language picker to render before focusing.
    afterNextRender(this, () => {
      this.$.sourceLanguagePickerBackButton.focus();
    });
  }

  private onTargetLanguageButtonClick() {
    this.targetLanguageMenuVisible = !this.targetLanguageMenuVisible;
    this.sourceLanguageMenuVisible = false;
    // We need to wait for the language picker to render before focusing.
    afterNextRender(this, () => {
      this.$.targetLanguagePickerBackButton.focus();
    });
  }

  private onSourceLanguageMenuItemClick(event: PointerEvent) {
    assertInstanceof(event.target, HTMLElement);
    const newSourceLanguage =
        this.$.sourceLanguagePickerContainer.itemForElement(event.target);
    this.sourceLanguage = newSourceLanguage;
    this.hideLanguagePickerMenus();
    this.maybeIssueTranslateRequest();
    recordLensOverlayInteraction(
        INVOCATION_SOURCE, UserAction.kTranslateSourceLanguageChanged);
  }

  private onTargetLanguageMenuItemClick(event: PointerEvent) {
    assertInstanceof(event.target, HTMLElement);
    const newTargetLanguage =
        this.$.targetLanguagePickerContainer.itemForElement(event.target);
    this.targetLanguage = newTargetLanguage;
    this.hideLanguagePickerMenus();
    this.maybeIssueTranslateRequest();
    recordLensOverlayInteraction(
        INVOCATION_SOURCE, UserAction.kTranslateTargetLanguageChanged);
    // Dispatch event to let other components know the overlay translate mode
    // state.
    this.dispatchEvent(new CustomEvent('translate-mode-state-changed', {
      bubbles: true,
      composed: true,
      detail: {
        translateModeEnabled: this.isTranslateModeEnabled,
        targetLanguage: this.targetLanguage.code,
        shouldUnselectWords: true,
      },
    }));
  }

  private onTranslateButtonClick() {
    // Toggle translate mode on button click.
    this.isTranslateModeEnabled = !this.isTranslateModeEnabled;
    if (this.isTranslateModeEnabled) {
      this.browserProxy.handler.maybeCloseTranslateFeaturePromo();
      this.maybeIssueTranslateRequest();
    } else {
      this.browserProxy.handler.issueEndTranslateModeRequest();
    }
    recordLensOverlayInteraction(
        INVOCATION_SOURCE,
        this.isTranslateModeEnabled ? UserAction.kTranslateButtonEnableAction :
                                      UserAction.kTranslateButtonDisableAction);

    // Focus or unfocus the shimmer depending on whether translate was
    // enabled/disabled.
    if (this.isTranslateModeEnabled) {
      focusShimmerOnRegion(
          this, /*top=*/ 0, /*left=*/ 0, /*width=*/ 0, /*height=*/ 0,
          ShimmerControlRequester.TRANSLATE);
      this.$.sourceLanguageButton.focus();
    } else {
      unfocusShimmer(this, ShimmerControlRequester.TRANSLATE);
      this.$.translateEnableButton.focus();
    }

    // Dispatch event to let other components know the overlay translate mode
    // state.
    this.dispatchEvent(new CustomEvent('translate-mode-state-changed', {
      bubbles: true,
      composed: true,
      detail: {
        translateModeEnabled: this.isTranslateModeEnabled,
        targetLanguage: this.targetLanguage.code,
        shouldUnselectWords: true,
      },
    }));
  }

  private maybeIssueTranslateRequest() {
    if (this.isTranslateModeEnabled && this.targetLanguage) {
      this.browserProxy.handler.issueTranslateFullPageRequest(
          this.sourceLanguage ? this.sourceLanguage.code : 'auto',
          this.targetLanguage.code);
    }
  }

  private setTranslateMode(sourceLanguage: string, targetLanguage: string) {
    if (sourceLanguage.length === 0 && targetLanguage.length === 0) {
      this.disableTranslateMode();
      return;
    }

    const newSourceLanguage = sourceLanguage === 'auto' ?
        null :
        this.translateLanguageList.find(
            language => language.code === sourceLanguage);
    const newTargetLanguage = this.translateLanguageList.find(
        language => language.code === targetLanguage);

    // Do nothing if the languages set are not in the language list. Source
    // language can be null to indicate we should auto-detect source language.
    // Also, do nothing if we set the translate mode to the same source and
    // target language that already appear on the screen.
    if (newSourceLanguage === undefined || !newTargetLanguage ||
        (this.sourceLanguage === newSourceLanguage &&
         this.targetLanguage === newTargetLanguage &&
         this.isTranslateModeEnabled)) {
      return;
    }

    this.sourceLanguage = newSourceLanguage;
    this.targetLanguage = newTargetLanguage;
    // Refocus the shimmer into translate mode if it was not already.
    if (!this.isTranslateModeEnabled) {
      focusShimmerOnRegion(
          this, /*top=*/ 0, /*left=*/ 0, /*width=*/ 0, /*height=*/ 0,
          ShimmerControlRequester.TRANSLATE);
    }
    this.isTranslateModeEnabled = true;
    this.maybeIssueTranslateRequest();
    this.dispatchEvent(new CustomEvent('translate-mode-state-changed', {
      bubbles: true,
      composed: true,
      detail: {
        translateModeEnabled: this.isTranslateModeEnabled,
        targetLanguage: this.targetLanguage.code,
        shouldUnselectWords: true,
      },
    }));
  }

  private disableTranslateMode() {
    if (!this.isTranslateModeEnabled) {
      return;
    }

    this.isTranslateModeEnabled = false;
    unfocusShimmer(this, ShimmerControlRequester.TRANSLATE);
    this.dispatchEvent(new CustomEvent('translate-mode-state-changed', {
      bubbles: true,
      composed: true,
      detail: {
        translateModeEnabled: this.isTranslateModeEnabled,
        targetLanguage: this.targetLanguage.code,
        shouldUnselectWords: false,
      },
    }));
  }

  private hideLanguagePickerMenus() {
    this.$.sourceLanguagePickerMenu.scroll(0, 0);
    this.$.targetLanguagePickerMenu.scroll(0, 0);
    // Depending on which language picker menu was opened, return focus to
    // the corresponding language button.
    if (this.sourceLanguageMenuVisible) {
      this.$.sourceLanguageButton.focus();
    } else {
      this.$.targetLanguageButton.focus();
    }

    this.targetLanguageMenuVisible = false;
    this.sourceLanguageMenuVisible = false;
  }

  private getSourceLanguageDisplayName(): string {
    if (this.sourceLanguage) {
      return this.sourceLanguage.displayName;
    }
    // There is a race condition where the DOM can render before the language
    // browser proxy returns the language list. For this reason, we need to
    // check if the translate language list is present before attempting to find
    // the content language display name inside of it.
    if (this.contentLanguage !== '' && this.translateLanguageList) {
      const detectedLanguage = this.translateLanguageList.find(
          language => language.code === this.contentLanguage);
      if (detectedLanguage !== undefined) {
        return detectedLanguage.displayName;
      }
    }
    return loadTimeData.getString('detectLanguage');
  }

  private getTargetLanguageDisplayName(): string {
    if (this.targetLanguage) {
      return this.targetLanguage.displayName;
    }

    return '';
  }

  private getContentLanguageDisplayName(): string {
    // There is a race condition where the DOM can render before the language
    // browser proxy returns the language list. For this reason, we need to
    // check if the translate language list is present before attempting to find
    // the content language display name inside of it.
    if (this.contentLanguage !== '' && this.translateLanguageList) {
      const detectedLanguage = this.translateLanguageList.find(
          language => language.code === this.contentLanguage);
      if (detectedLanguage !== undefined) {
        return detectedLanguage.displayName;
      }
    }
    return '';
  }

  private computeShouldShowStarsIcon(): boolean {
    return this.sourceLanguage === null;
  }

  private getTabIndexForTranslateEntry(): number {
    return this.isTranslateModeEnabled ? -1 : 0;
  }

  private getTabIndexForLanguagePickerButtons(): number {
    return this.computeLanguagePickerButtonsVisible() ? 0 : -1;
  }

  private computeLanguagePickerButtonsVisible(): boolean {
    return this.isTranslateModeEnabled && !this.sourceLanguageMenuVisible &&
        !this.targetLanguageMenuVisible;
  }

  private getSourceLanguageButtonAriaLabel(): string {
    return loadTimeData.getStringF(
        'sourceLanguageAriaLabel', this.getSourceLanguageDisplayName());
  }

  private getTargetLanguageButtonAriaLabel(): string {
    return loadTimeData.getStringF(
        'targetLanguageAriaLabel', this.getTargetLanguageDisplayName());
  }

  private getAutoCheckedClass(
      sourceLanguage: chrome.languageSettingsPrivate.Language): string {
    return sourceLanguage === null ? 'selected' : '';
  }

  private getLanguageCheckedClass(
      language: chrome.languageSettingsPrivate.Language,
      selectedLanguage: chrome.languageSettingsPrivate.Language): string {
    return selectedLanguage === language ? 'selected' : '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'translate-button': TranslateButtonElement;
  }
}

customElements.define(TranslateButtonElement.is, TranslateButtonElement);
