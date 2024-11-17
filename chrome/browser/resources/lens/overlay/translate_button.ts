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

interface SearchedLanguage {
  beforeHighlightText: string;
  searchHighlightText: string;
  afterHighlightText: string;
  language: Language;
}

export interface TranslateState {
  translateModeEnabled: boolean;
  targetLanguage: string;
  shouldUnselectWords: boolean;
}

export interface TranslateButtonElement {
  $: {
    allSourceLanguagesMenu: HTMLDivElement,
    allTargetLanguagesMenu: HTMLDivElement,
    menuDetectedLanguage: HTMLDivElement,
    languagePicker: HTMLDivElement,
    recentSourceLanguagesContainer: DomRepeat,
    recentSourceLanguagesSection: HTMLDivElement,
    recentTargetLanguagesContainer: DomRepeat,
    recentTargetLanguagesSection: HTMLDivElement,
    searchSourceLanguagesContainer: DomRepeat,
    searchSourceLanguagePicker: HTMLDivElement,
    searchTargetLanguagesContainer: DomRepeat,
    searchTargetLanguagePicker: HTMLDivElement,
    sourceAutoDetectButton: CrButtonElement,
    sourceLanguageButton: CrButtonElement,
    sourceLanguagePickerBackButton: CrIconButtonElement,
    sourceLanguagePickerContainer: DomRepeat,
    sourceLanguagePickerMenu: HTMLDivElement,
    sourceLanguageSearchButton: CrIconButtonElement,
    sourceLanguageSearchbox: HTMLInputElement,
    targetLanguageButton: CrButtonElement,
    targetLanguagePickerBackButton: CrIconButtonElement,
    targetLanguagePickerContainer: DomRepeat,
    targetLanguagePickerMenu: HTMLDivElement,
    targetLanguageSearchButton: CrIconButtonElement,
    targetLanguageSearchbox: HTMLInputElement,
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
      isSourceLanguageSearchboxOpen: {
        type: Boolean,
        reflectToAttribute: true,
      },
      isTargetLanguageSearchboxOpen: {
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
      recentSourceLanguages: Array,
      recentTargetLanguages: Array,
      shouldFetchSupportedLanguages: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('shouldFetchSupportedLanguages'),
        readOnly: true,
        reflectToAttribute: true,
      },
      shouldShowRecentSourceLanguages: {
        type: Boolean,
        computed: 'shouldShowRecentLanguages(recentSourceLanguages)',
        reflectToAttribute: true,
      },
      shouldShowRecentTargetLanguages: {
        type: Boolean,
        computed: 'shouldShowRecentLanguages(recentTargetLanguages)',
        reflectToAttribute: true,
      },
      searchboxHasText: {
        type: Boolean,
        reflectToAttribute: true,
      },
      searchedLanguageList: Array,
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
  // Whether the source language searchbox is currently open.
  private isSourceLanguageSearchboxOpen: boolean = false;
  // Whether the target language searchbox is currently open.
  private isTargetLanguageSearchboxOpen: boolean = false;
  // Whether the translate mode on the lens overlay has been enabled.
  private isTranslateModeEnabled: boolean = false;
  // Whether the language picker buttons are currently visible.
  private languagePickerButtonsVisible: boolean;
  // Whether the feature flag to enable fetching supported languages is enabled.
  private shouldFetchSupportedLanguages: boolean;
  // Whether either of the language picker searchboxes has text.
  private searchboxHasText: boolean = false;
  // The list of languages filtered by a search highlight. Uses a
  // `SearchedLanguage` so we can bold the search highlight.
  private searchedLanguageList: SearchedLanguage[] = [];
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
  // The recent languages that the user has selected as source language.
  private recentSourceLanguages: Language[] = [];
  // The recent languages that the user has selected as target language.
  private recentTargetLanguages: Language[] = [];
  // Whether we should show the recent source languages in the picker.
  private shouldShowRecentSourceLanguages: boolean = false;
  // Whether we should show the recent target languages in the picker.
  private shouldShowRecentTargetLanguages: boolean = false;
  // The content language code received from the text layer.
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
    // If the searchbox is open, then we should do nothing in this case.
    if (this.isTargetLanguageSearchboxOpen ||
        this.isSourceLanguageSearchboxOpen) {
      return;
    }

    // A language picker must be focused and visible in order to receive this
    // event.
    assert(this.sourceLanguageMenuVisible || this.targetLanguageMenuVisible);
    // The key must be of length 1 if it is a character.
    if (event.key.length !== 1) {
      return;
    }

    // We should open the searchbox with the key if it is active.
    if (this.shouldFetchSupportedLanguages) {
      const isTarget = this.targetLanguageMenuVisible ? true : false;
      if (isTarget) {
        this.openTargetLanguageSearchbox();
      } else {
        this.openSourceLanguageSearchbox();
      }
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
    if (this.shouldFetchSupportedLanguages) {
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
        this.shouldFetchSupportedLanguages ? this.supportedSourceLanguages :
                                             SUPPORTED_TRANSLATION_LANGUAGES;
    const supportedTargetTranslateLanguages =
        this.shouldFetchSupportedLanguages ? this.supportedTargetLanguages :
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
    if (this.shouldFetchSupportedLanguages) {
      // Set the recent languages for source and target language pickers. If
      // there are none, this is a no-op.
      this.maybeSetRecentLanguages();
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

  private maybeSetRecentLanguages() {
    const recentSourceLanguageCodes =
        this.languageBrowserProxy.getRecentSourceLanguages();
    if (recentSourceLanguageCodes.length > 0) {
      this.recentSourceLanguages =
          this.getSourceLanguageList().filter((language: Language) => {
            return recentSourceLanguageCodes.includes(language.languageCode);
          });
    }

    const recentTargetLanguageCodes =
        this.languageBrowserProxy.getRecentTargetLanguages();
    if (recentTargetLanguageCodes.length > 0) {
      this.recentTargetLanguages =
          this.getTargetLanguageList().filter((language: Language) => {
            return recentTargetLanguageCodes.includes(language.languageCode);
          });
    }
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
    this.setNewSourceLanguage(newSourceLanguage);
  }

  private onRecentSourceLanguageClick(event: PointerEvent) {
    assertInstanceof(event.target, HTMLElement);
    const newSourceLanguage =
        this.$.recentSourceLanguagesContainer.itemForElement(event.target);
    this.setNewSourceLanguage(newSourceLanguage);
  }

  private onSourceSearchLanguageItemClick(event: PointerEvent) {
    assertInstanceof(event.target, HTMLElement);
    const searchedLanguage =
        this.$.searchSourceLanguagesContainer.itemForElement(event.target);
    this.setNewSourceLanguage(searchedLanguage.language);
  }

  private setNewSourceLanguage(sourceLanguage: Language|null) {
    this.sourceLanguage = sourceLanguage;
    if (this.shouldFetchSupportedLanguages) {
      this.languageBrowserProxy.storeLastUsedSourceLanguage(
          sourceLanguage ? sourceLanguage.languageCode : null);
      this.addRecentSourceLanguage(sourceLanguage);
    }
    this.clearSearchboxState();
    this.hideLanguagePickerMenus();
    this.maybeIssueTranslateRequest();
    recordLensOverlayInteraction(
        INVOCATION_SOURCE, UserAction.kTranslateSourceLanguageChanged);
  }

  private onTargetLanguageMenuItemClick(event: PointerEvent) {
    assertInstanceof(event.target, HTMLElement);
    const newTargetLanguage =
        this.$.targetLanguagePickerContainer.itemForElement(event.target);
    this.setNewTargetLanguage(newTargetLanguage);
  }

  private onTargetSearchLanguageItemClick(event: PointerEvent) {
    assertInstanceof(event.target, HTMLElement);
    const searchedLanguage =
        this.$.searchTargetLanguagesContainer.itemForElement(event.target);
    this.setNewTargetLanguage(searchedLanguage.language);
  }

  private onRecentTargetLanguageClick(event: PointerEvent) {
    assertInstanceof(event.target, HTMLElement);
    const newTargetLanguage =
        this.$.recentTargetLanguagesContainer.itemForElement(event.target);
    this.setNewTargetLanguage(newTargetLanguage);
  }

  private setNewTargetLanguage(targetLanguage: Language) {
    this.targetLanguage = targetLanguage;
    if (this.shouldFetchSupportedLanguages) {
      this.languageBrowserProxy.storeLastUsedTargetLanguage(
          targetLanguage ? targetLanguage.languageCode : null);
      this.addRecentTargetLanguage(targetLanguage);
    }
    this.clearSearchboxState();
    this.hideLanguagePickerMenus();
    this.maybeIssueTranslateRequest();
    recordLensOverlayInteraction(
        INVOCATION_SOURCE, UserAction.kTranslateTargetLanguageChanged);
    this.updateTranslateModeState(true);
  }

  private onTranslateButtonClick() {
    // Toggle translate mode on button click.
    this.isTranslateModeEnabled = !this.isTranslateModeEnabled;
    if (this.isTranslateModeEnabled) {
      this.browserProxy.handler.maybeCloseTranslateFeaturePromo(
          /*featureEngaged=*/ true);
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
    this.maybeIssueTranslateRequest();
    this.updateTranslateModeState(true);
  }

  private disableTranslateMode() {
    if (!this.isTranslateModeEnabled) {
      return;
    }

    this.isTranslateModeEnabled = false;
    unfocusShimmer(this, ShimmerControlRequester.TRANSLATE);
    this.updateTranslateModeState(false);
  }

  private handleBackButtonClick() {
    if (this.isTargetLanguageSearchboxOpen ||
        this.isSourceLanguageSearchboxOpen) {
      this.clearSearchboxState();
      return;
    }

    this.hideLanguagePickerMenus();
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

  private openSourceLanguageSearchbox() {
    this.isSourceLanguageSearchboxOpen = true;
    this.$.sourceLanguageSearchbox.focus();
    this.onSourceSearchboxInputChange();
  }

  private openTargetLanguageSearchbox() {
    this.isTargetLanguageSearchboxOpen = true;
    this.$.targetLanguageSearchbox.focus();
    this.onTargetSearchboxInputChange();
  }

  private onSourceSearchboxInputChange() {
    const searchString = this.$.sourceLanguageSearchbox.value.trim();
    this.updateSearchedListOnInputChanged(
        this.getSourceLanguageList(), searchString);
  }

  private onTargetSearchboxInputChange() {
    const searchString = this.$.targetLanguageSearchbox.value.trim();
    this.updateSearchedListOnInputChanged(
        this.getTargetLanguageList(), searchString);
  }

  private updateSearchedListOnInputChanged(
      languageList: Language[], searchString: string) {
    this.searchboxHasText = searchString.length > 0;
    if (this.searchboxHasText) {
      const lowerCaseSearchString = searchString.toLowerCase();
      this.searchedLanguageList =
          languageList
              .filter((language: Language) => {
                const lowerCaseLanguageName = language.name.toLowerCase();
                return lowerCaseLanguageName.includes(lowerCaseSearchString);
              })
              .sort((previous: Language, next: Language) => {
                const lowerCaseA = previous.name.toLowerCase();
                const lowerCaseB = next.name.toLowerCase();
                const startsWithA =
                    lowerCaseA.startsWith(lowerCaseSearchString);
                const startsWithB =
                    lowerCaseB.startsWith(lowerCaseSearchString);

                if (startsWithA && !startsWithB) {
                  return -1;
                } else if (!startsWithA && startsWithB) {
                  return 1;
                } else {
                  return 0;
                }
              })
              .map((language: Language) => {
                const lowerCaseLanguageName = language.name.toLowerCase();
                const startIndex =
                    lowerCaseLanguageName.indexOf(lowerCaseSearchString);
                if (startIndex !== -1) {
                  const beforeSearch = language.name.substring(0, startIndex);
                  const highlightText = language.name.substring(
                      startIndex, startIndex + lowerCaseSearchString.length);
                  const afterSearch = language.name.substring(
                      startIndex + lowerCaseSearchString.length);
                  return {
                    beforeHighlightText: beforeSearch,
                    searchHighlightText: highlightText,
                    afterHighlightText: afterSearch,
                    language,
                  };
                }
                return {
                  beforeHighlightText: language.name,
                  searchHighlightText: '',
                  afterHighlightText: '',
                  language,
                };
              });
    }
  }

  private clearSearchboxState() {
    this.searchedLanguageList = [];
    this.$.targetLanguageSearchbox.value = '';
    this.$.sourceLanguageSearchbox.value = '';
    this.searchboxHasText = false;
    this.isSourceLanguageSearchboxOpen = false;
    this.isTargetLanguageSearchboxOpen = false;
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

  private getSearchedLanguageCheckedClass(
      searchedLanguage: SearchedLanguage,
      selectedLanguage: Language|null): string {
    return this.getLanguageCheckedClass(
        searchedLanguage.language, selectedLanguage);
  }

  private getLanguageCheckedClass(
      language: Language, selectedLanguage: Language|null): string {
    if (!selectedLanguage) {
      return '';
    }

    return selectedLanguage.languageCode === language.languageCode ?
        'selected' :
        '';
  }

  private shouldShowRecentLanguages(recentLanguages: Language[]) {
    return recentLanguages.length > 0;
  }

  private updateTranslateModeState(shouldUnselectWords: boolean) {
    this.dispatchEvent(new CustomEvent('translate-mode-state-changed', {
      bubbles: true,
      composed: true,
      detail: {
        translateModeEnabled: this.isTranslateModeEnabled,
        targetLanguage: this.targetLanguage!.languageCode,
        shouldUnselectWords: shouldUnselectWords,
      },
    }));
  }

  private addRecentSourceLanguage(language: Language|null) {
    if (!language) {
      return;
    }
    this.prepareRecentLanguageListForAddition(
        this.recentSourceLanguages, language);

    // This needs to happen this way in order for properties to properly update
    // the HTML.
    this.recentSourceLanguages = [language, ...this.recentSourceLanguages];
    this.languageBrowserProxy.storeRecentSourceLanguages(
        this.recentSourceLanguages.map((language: Language) => {
          return language.languageCode;
        }));
  }

  private addRecentTargetLanguage(language: Language) {
    assert(language);
    this.prepareRecentLanguageListForAddition(
        this.recentTargetLanguages, language);

    // This needs to happen this way in order for properties to properly update
    // the HTML.
    this.recentTargetLanguages = [language, ...this.recentTargetLanguages];
    this.languageBrowserProxy.storeRecentTargetLanguages(
        this.recentTargetLanguages.map((language: Language) => {
          return language.languageCode;
        }));
  }

  // Prepares the recent language list for addition by removing the language if
  // it is already in the list or popping languages if its length is above the
  // max.
  private prepareRecentLanguageListForAddition(
      languageList: Language[], language: Language) {
    // If the language is already present in the queue, remove it and then
    // re-add it so it's at the top. If the slots are full, then we should
    // dequeue languages until it is not and then add the most recent language.
    const index = languageList.findIndex(
        (recentLanguage: Language) =>
            recentLanguage.languageCode === language.languageCode);

    if (index > -1) {
      languageList.splice(index, 1);
    }
    while (languageList.length >=
           loadTimeData.getInteger('recentLanguagesAmount')) {
      languageList.pop();
    }
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
