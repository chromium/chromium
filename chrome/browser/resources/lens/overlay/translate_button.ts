// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

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
import type {Language} from './translate.mojom-webui.js';
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
    translateContainer: HTMLDivElement,
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
      isLensOverlayContextualSearchboxEnabled: {
        type: Boolean,
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
      shouldHideLanguagePicker: {
        type: Boolean,
        reflectToAttribute: true,
      },
      shouldShowStarsIcon: {
        type: Boolean,
        computed: 'computeShouldShowStarsIcon(sourceLanguage)',
        reflectToAttribute: true,
      },
      sourceLanguage: Object,
      sourceLanguageList: {
        type: Array,
        computed: `getSourceLanguageList(clientSourceLanguageList,
                                   serverSourceLanguageList)`,
      },
      sourceLanguageMenuVisible: {
        type: Boolean,
        reflectToAttribute: true,
      },
      targetLanguage: Object,
      targetLanguageList: {
        type: Array,
        computed: `getTargetLanguageList(clientTargetLanguageList,
                                   serverTargetLanguageList)`,
      },
      targetLanguageMenuVisible: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  private eventTracker: EventTracker = new EventTracker();
  // Whether the lens overlay contextual searchbox is enabled. Passed in from
  // parent.
  private isLensOverlayContextualSearchboxEnabled: boolean;
  // Whether the translate mode on the lens overlay has been enabled.
  private isTranslateModeEnabled: boolean = false;
  // Whether the language picker buttons are currently visible.
  private languagePickerButtonsVisible: boolean;
  // Whether the stars icon is visible on the source language button.
  private shouldShowStarsIcon: boolean;
  // The currently selected source language to translate to. If null, we should
  // auto detect the language.
  private sourceLanguage: Language|null = null;
  // The currently selected target language to translate to.
  private targetLanguage: Language|null = null;
  // Whether the source language menu picker is visible.
  private sourceLanguageMenuVisible: boolean = false;
  // Whether the target language menu picker is visible.
  private targetLanguageMenuVisible: boolean = false;
  // The list of source translate language codes supported by Lens. This differs
  // from the server source translate list because it is a list of language
  // codes that can currently be reliably sent to Lens for translation.
  private supportedSourceLanguages: Set<string> =
      new Set(loadTimeData.getString('translateSourceLanguages').split(','));
  // The list of target translate language codes supported by Lens. This differs
  // from the server target translate list because it is a list of language
  // codes that can currently be reliably sent to Lens for translation. This set
  // needs to be combined with `supportedSourceLanguages` before use.
  private supportedTargetLanguages: Set<string> =
      new Set(loadTimeData.getString('translateTargetLanguages').split(','));
  // The list of source translate languages provided by the chrome API.
  private clientSourceLanguageList: Language[];
  // The list of source translate languages provided by the chrome API.
  private clientTargetLanguageList: Language[];
  // The list of source translate languages provided by the server.
  private serverSourceLanguageList: Language[] = [];
  // The list of target translate languages provided by the server.
  private serverTargetLanguageList: Language[] = [];
  // The content language code received from the lext layer.
  private contentLanguage: string = '';
  // Whether we should hide the language picker.
  private shouldHideLanguagePicker: boolean = false;
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
    this.handleFetchLanguageList();
    this.eventTracker.add(
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
    this.eventTracker.add(
        this.$.sourceLanguagePickerMenu, 'focusout', (event: FocusEvent) => {
          const targetWithFocus = event.relatedTarget;
          if (!targetWithFocus || !(targetWithFocus instanceof Node) ||
              !this.$.sourceLanguagePickerMenu.contains(targetWithFocus)) {
            this.hideLanguagePickerMenus(/*shouldFocus=*/ false);
          }
        });
    this.eventTracker.add(
        this.$.targetLanguagePickerMenu, 'focusout', (event: FocusEvent) => {
          const targetWithFocus = event.relatedTarget;
          if (!targetWithFocus || !(targetWithFocus instanceof Node) ||
              !this.$.targetLanguagePickerMenu.contains(targetWithFocus)) {
            this.hideLanguagePickerMenus(/*shouldFocus=*/ false);
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
    this.eventTracker.removeAll();
  }

  getTranslateEnableButton(): CrButtonElement {
    return this.$.translateEnableButton;
  }

  private handleLanguagePickerKeyDown(event: KeyboardEvent) {
    // A language picker must be focused and visible in order to receive this
    // event.
    assert(this.sourceLanguageMenuVisible || this.targetLanguageMenuVisible);
    // The key must be of length 1 if it is a character.
    if (event.key.length !== 1) {
      return;
    }

    // Get the appropriate language list by checking which language menu is
    // visible.
    const languageList = this.sourceLanguageMenuVisible ?
        this.getSourceLanguageList() :
        this.getTargetLanguageList();

    let scrollLanguageIndex = -1;
    const startingChar = event.key.toLowerCase();
    for (let i = 0; i < languageList.length; i++) {
      const language = languageList[i];
      const languageStartingChar = language.name.charAt(0).toLowerCase();
      if (startingChar === languageStartingChar) {
        scrollLanguageIndex = i;
        break;
      }
    }

    if (scrollLanguageIndex >= 0) {
      const pickerMenu = this.sourceLanguageMenuVisible ?
          this.$.sourceLanguagePickerMenu :
          this.$.targetLanguagePickerMenu;
      const menuItems = pickerMenu.querySelectorAll<CrButtonElement>(
          'cr-button:not(#sourceAutoDetectButton)');
      const languageElement = menuItems[scrollLanguageIndex];
      languageElement.scrollIntoView();
      languageElement.focus();
    }
  }

  private handleFetchLanguageList() {
    if (loadTimeData.getBoolean('shouldFetchSupportedLanguages')) {
      // Combine the source and target translate languages into one set.
      this.supportedSourceLanguages.forEach(
          (code: string) => this.supportedTargetLanguages.add(code));
      this.languageBrowserProxy.getStoredServerLanguages(this.browserProxy)
          .then(this.onServerLanguageListRetrieved.bind(this));
    }

    this.languageBrowserProxy.getClientLanguageList().then(
        this.onClientLanguageListRetrieved.bind(this));
  }

  private onServerLanguageListRetrieved(
      languages: {sourceLanguages: Language[], targetLanguages: Language[]}) {
    this.serverSourceLanguageList =
        languages.sourceLanguages.filter((language) => {
          return this.supportedSourceLanguages.has(language.languageCode);
        });

    this.serverTargetLanguageList =
        languages.targetLanguages.filter((language) => {
          return this.supportedTargetLanguages.has(language.languageCode);
        });

    // Since we always want to use the server languages over the client
    // languages, we set a variable to always override any existing language if
    // the initial languages were already set.
    this.maybeSetInitialLanguagesInPicker(/*overrideExisting=*/ true);
  }

  private onClientLanguageListRetrieved(languageList: Language[]) {
    const supportedSourceTranslateLanguages =
        loadTimeData.getBoolean('shouldFetchSupportedLanguages') ?
        this.supportedSourceLanguages :
        SUPPORTED_TRANSLATION_LANGUAGES;
    const supportedTargetTranslateLanguages =
        loadTimeData.getBoolean('shouldFetchSupportedLanguages') ?
        this.supportedTargetLanguages :
        SUPPORTED_TRANSLATION_LANGUAGES;
    this.clientSourceLanguageList = languageList.filter((language) => {
      return supportedSourceTranslateLanguages.has(language.languageCode);
    });
    this.clientTargetLanguageList = languageList.filter((language) => {
      return supportedTargetTranslateLanguages.has(language.languageCode);
    });

    this.maybeSetInitialLanguagesInPicker();
  }

  private maybeSetInitialLanguagesInPicker(overrideExisting = false) {
    // If the target language was already set and we do not want to override
    // anyway, then we should return early.
    if (this.targetLanguage && !overrideExisting) {
      return;
    }

    // Last used source and target languages are stored in local storage if
    // feature enabled.
    if (loadTimeData.getBoolean('shouldFetchSupportedLanguages')) {
      const sourceLanguageCode =
          this.languageBrowserProxy.getLastUsedSourceLanguage();
      const targetLanguageCode =
          this.languageBrowserProxy.getLastUsedTargetLanguage();
      const initialSourceLanguage = this.getSourceLanguageList().find(
          language => language.languageCode === sourceLanguageCode);
      const initialTargetLanguage = this.getTargetLanguageList().find(
          language => language.languageCode === targetLanguageCode);

      this.sourceLanguage =
          initialSourceLanguage ? initialSourceLanguage : null;
      this.targetLanguage =
          initialTargetLanguage ? initialTargetLanguage : null;
      // If target language was still not set, then we still need to get the
      // translate target language from the language browser proxy. Otherwise,
      // return.
      if (this.targetLanguage) {
        return;
      }
    }

    // Get the default translate target language. This needs to happen after
    // fetching the language list so we can use the list to fetch the language's
    // display name.
    this.languageBrowserProxy.getTranslateTargetLanguage().then(
        this.onTargetLanguageRetrieved.bind(this));
  }

  private onTargetLanguageRetrieved(targetLanguageCode: string) {
    const defaultLanguage = this.getTargetLanguageList().find(
        language => language.languageCode === targetLanguageCode);

    // If the target language is set to one supported by Lens, then we set it
    // and are done.
    if (defaultLanguage) {
      this.targetLanguage = defaultLanguage;
      return;
    }

    // Otherwise, we default to the first language in the list.
    this.targetLanguage = this.getTargetLanguageList()[0];
  }

  private onAutoDetectMenuItemClick() {
    this.sourceLanguage = null;
    this.languageBrowserProxy.storeLastUsedSourceLanguage(null);
    this.hideLanguagePickerMenus();
    this.maybeIssueTranslateRequest();
    recordLensOverlayInteraction(
        INVOCATION_SOURCE, UserAction.kTranslateSourceLanguageChanged);
  }

  private onSourceLanguageButtonClick() {
    this.notifyLanguagePickerOpened();
    this.sourceLanguageMenuVisible = !this.sourceLanguageMenuVisible;
    this.targetLanguageMenuVisible = false;
    // We need to wait for the language picker to render before focusing.
    afterNextRender(this, () => {
      this.$.sourceLanguagePickerBackButton.focus();
    });
  }

  private onTargetLanguageButtonClick() {
    this.notifyLanguagePickerOpened();
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
    this.languageBrowserProxy.storeLastUsedSourceLanguage(
        newSourceLanguage ? newSourceLanguage.languageCode : null);
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
    this.languageBrowserProxy.storeLastUsedTargetLanguage(
        newTargetLanguage ? newTargetLanguage.languageCode : null);
    this.hideLanguagePickerMenus();
    this.maybeIssueTranslateRequest();
    recordLensOverlayInteraction(
        INVOCATION_SOURCE, UserAction.kTranslateTargetLanguageChanged);
    // Dispatch event to let other components know the overlay translate mode
    // state.
    this.updateTranslateModeState(true);
  }

  private onTranslateButtonClick() {
    // Toggle translate mode on button click.
    this.isTranslateModeEnabled = !this.isTranslateModeEnabled;
    if (this.isTranslateModeEnabled) {
      this.addOnHoverListeners();
      this.browserProxy.handler.maybeCloseTranslateFeaturePromo(
          /*featureEngaged=*/ true);
      this.maybeIssueTranslateRequest();
    } else {
      this.removeOnHoverListeners();
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
    this.updateTranslateModeState(true);
  }

  private maybeIssueTranslateRequest() {
    if (this.isTranslateModeEnabled && this.targetLanguage) {
      this.browserProxy.handler.issueTranslateFullPageRequest(
          this.sourceLanguage ? this.sourceLanguage.languageCode : 'auto',
          this.targetLanguage.languageCode);
    }
  }

  private setTranslateMode(sourceLanguage: string, targetLanguage: string) {
    if (sourceLanguage.length === 0 && targetLanguage.length === 0) {
      this.disableTranslateMode();
      return;
    }

    const newSourceLanguage = sourceLanguage === 'auto' ?
        null :
        this.getSourceLanguageList().find(
            language => language.languageCode === sourceLanguage);
    const newTargetLanguage = this.getTargetLanguageList().find(
        language => language.languageCode === targetLanguage);

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
    this.addOnHoverListeners();
    this.maybeIssueTranslateRequest();
    this.updateTranslateModeState(true);
  }

  private disableTranslateMode() {
    if (!this.isTranslateModeEnabled) {
      return;
    }

    this.removeOnHoverListeners();
    this.isTranslateModeEnabled = false;
    unfocusShimmer(this, ShimmerControlRequester.TRANSLATE);
    this.updateTranslateModeState(false);
  }

  private hideLanguagePickerMenus(shouldFocus = true) {
    this.$.sourceLanguagePickerMenu.scroll(0, 0);
    this.$.targetLanguagePickerMenu.scroll(0, 0);
    // Depending on which language picker menu was opened, return focus to
    // the corresponding language button.
    if (shouldFocus) {
      if (this.sourceLanguageMenuVisible) {
        this.$.sourceLanguageButton.focus();
      } else {
        this.$.targetLanguageButton.focus();
      }
    }

    this.notifyLanguagePickerClosed();
    this.targetLanguageMenuVisible = false;
    this.sourceLanguageMenuVisible = false;
  }

  private getSourceLanguageDisplayName(): string {
    if (this.sourceLanguage) {
      return this.sourceLanguage.name;
    }
    // There is a race condition where the DOM can render before the language
    // browser proxy returns the language list. For this reason, we need to
    // check if the translate language list is present before attempting to find
    // the content language display name inside of it.
    if (this.contentLanguage !== '' && this.getSourceLanguageList()) {
      const detectedLanguage = this.getSourceLanguageList().find(
          language => language.languageCode === this.contentLanguage);
      if (detectedLanguage !== undefined) {
        return detectedLanguage.name;
      }
    }
    return loadTimeData.getString('detectLanguage');
  }

  private getTargetLanguageDisplayName(): string {
    if (this.targetLanguage) {
      return this.targetLanguage.name;
    }

    return '';
  }

  private getContentLanguageDisplayName(): string {
    // There is a race condition where the DOM can render before the language
    // browser proxy returns the language list. For this reason, we need to
    // check if the translate language list is present before attempting to find
    // the content language display name inside of it.
    if (this.contentLanguage !== '' && this.getSourceLanguageList()) {
      const detectedLanguage = this.getSourceLanguageList().find(
          language => language.languageCode === this.contentLanguage);
      if (detectedLanguage !== undefined) {
        return detectedLanguage.name;
      }
    }
    return '';
  }

  private notifyLanguagePickerOpened() {
    document.dispatchEvent(new CustomEvent('language-picker-opened', {
      bubbles: true,
      composed: true,
    }));
  }

  private notifyLanguagePickerClosed() {
    document.dispatchEvent(new CustomEvent('language-picker-closed', {
      bubbles: true,
      composed: true,
    }));
  }

  private computeShouldShowStarsIcon(): boolean {
    return this.sourceLanguage === null;
  }

  private getTabIndexForTranslateEntry(): number {
    return this.isTranslateModeEnabled ? -1 : 0;
  }

  private getTabIndexForTranslateExit(): number {
    if ((this.sourceLanguageMenuVisible || this.targetLanguageMenuVisible) &&
        this.isLensOverlayContextualSearchboxEnabled) {
      return 0;
    }

    return this.getTabIndexForLanguagePickerButtons();
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

  private getAutoCheckedClass(sourceLanguage: Language): string {
    return sourceLanguage === null ? 'selected' : '';
  }

  private getLanguageCheckedClass(
      language: Language, selectedLanguage: Language): string {
    return selectedLanguage === language ? 'selected' : '';
  }

  private updateTranslateModeState(shouldUnselectWords: boolean) {
    this.dispatchEvent(new CustomEvent('translate-mode-state-changed', {
      bubbles: true,
      composed: true,
      detail: {
        translateModeEnabled: this.isTranslateModeEnabled,
        targetLanguage: this.targetLanguage!.languageCode,
        shouldHideSearchbox:
            this.isTranslateModeEnabled && !this.shouldHideLanguagePicker,
        shouldUnselectWords: shouldUnselectWords,
      },
    }));
  }

  private addOnHoverListeners() {
    // We do not want to enable this functionality unless the contextual
    // searchbox is enabled.
    if (!this.isLensOverlayContextualSearchboxEnabled) {
      return;
    }

    this.eventTracker.add(this.$.translateContainer, 'mouseleave', () => {
      if (this.sourceLanguageMenuVisible || this.targetLanguageMenuVisible) {
        return;
      }

      this.shouldHideLanguagePicker = true;
      this.$.sourceLanguageButton.blur();
      this.$.targetLanguageButton.blur();
      this.updateTranslateModeState(false);
    });
    this.eventTracker.add(this.$.translateDisableButton, 'mouseover', () => {
      this.shouldHideLanguagePicker = false;
      this.updateTranslateModeState(false);
    });
    // We also need to track focus events for accessibility reasons.
    this.eventTracker.add(this.$.translateDisableButton, 'focus', () => {
      this.shouldHideLanguagePicker = false;
      this.updateTranslateModeState(false);
    });
  }

  private removeOnHoverListeners() {
    this.eventTracker.remove(this.$.translateContainer, 'mouseleave');
    this.eventTracker.remove(this.$.translateDisableButton, 'mouseover');
    this.shouldHideLanguagePicker = false;
  }

  setContextualSearchboxEnabledForTesting(enabled: boolean) {
    this.isLensOverlayContextualSearchboxEnabled = enabled;
  }

  private getSourceLanguageList(): Language[] {
    return this.serverSourceLanguageList.length === 0 ?
        this.clientSourceLanguageList :
        this.serverSourceLanguageList;
  }

  private getTargetLanguageList(): Language[] {
    return this.serverTargetLanguageList.length === 0 ?
        this.clientTargetLanguageList :
        this.serverTargetLanguageList;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'translate-button': TranslateButtonElement;
  }
}

customElements.define(TranslateButtonElement.is, TranslateButtonElement);
