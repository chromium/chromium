// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  createTranscriptionModelDownloadPerf,
  EventsSender,
} from './events_sender.js';
import {NoArgStringName} from './i18n.js';
import {InternalMicInfo} from './microphone_manager.js';
import {
  getModelUiOrder,
  ModelLoader,
  ModelState,
} from './on_device_model/types.js';
import {PerfLogger} from './perf.js';
import {effect, ReadonlySignal, Signal} from './reactive/signal.js';
import {LangPackInfo, LanguageCode} from './soda/language_info.js';
import {SodaSession} from './soda/types.js';
import {settings} from './state/settings.js';

export abstract class PlatformHandler {
  /**
   * Returns the formatted localized string by given `id` and `args`.
   *
   * This is the lower level function that is used to implement the `i18n`
   * helper in core/i18n.ts, and shouldn't be directly used.
   * The `i18n` helper provides better typing and should be used instead.
   *
   * This is declared as `static` so it can be directly use at module import
   * time, and all implementations should ensure that it can be called at
   * module import time.
   */
  static getStringF(_id: string, ..._args: Array<number|string>): string {
    throw new Error('getStringF not implemented');
  }

  /**
   * Returns device type.
   *
   * This is the lower level function that is used to replace get device type
   * string in core/i18n.ts, and shouldn't be directly used.
   *
   * This is declared as `static` so it can be directly use at module import
   * time, and all implementations should ensure that it can be called at
   * module import time.
   */
  static getDeviceType(): string {
    throw new Error('getDeviceType not implemented');
  }

  /**
   * Initializes the platform handler.
   *
   * This should only be called once when the app starts.
   */
  abstract init(): Promise<void>;

  /**
   * The model loader for summarization.
   */
  abstract summaryModelLoader: ModelLoader<string>;

  /**
   * The model loader for title suggestion.
   */
  abstract titleSuggestionModelLoader: ModelLoader<string[]>;

  /**
   * Gets integrated model state for title suggestion and summarization.
   *
   * Model state with smaller UI order will be returned. If both models are
   * downloading, then return state with smaller progress.
   */
  getGenAiModelState(): ModelState {
    const summaryModelState = this.summaryModelLoader.state.value;
    const summaryUiOrder = getModelUiOrder(summaryModelState);
    const titleSuggestionModelState =
      this.titleSuggestionModelLoader.state.value;
    const titleSuggestionUiOrder = getModelUiOrder(titleSuggestionModelState);

    if (summaryModelState.kind === 'installing' &&
        titleSuggestionModelState.kind === 'installing') {
      if (summaryModelState.progress < titleSuggestionModelState.progress) {
        return summaryModelState;
      }
      return titleSuggestionModelState;
    }

    if (summaryUiOrder < titleSuggestionUiOrder) {
      return summaryModelState;
    }
    return titleSuggestionModelState;
  }

  /**
   * Wrapper to download GenAI-related model.
   */
  downloadGenAiModel(): void {
    this.summaryModelLoader.download();
    this.titleSuggestionModelLoader.download();
  }

  /**
   * Returns the default language based on the application locale or profile
   * preference.
   *
   * Returns EN_US if default language is not available.
   */
  abstract getDefaultLanguage(): LanguageCode;

  /**
   * Returns a readonly list of language pack info.
   */
  abstract getLangPackList(): readonly LangPackInfo[];

  /**
   * Returns information of the given language.
   */
  abstract getLangPackInfo(language: LanguageCode): LangPackInfo;

  /**
   * Returns the currently selected language.
   *
   * Returns null when there are multiple available languages, and no language
   * is selected.
   */
  getSelectedLanguage(): LanguageCode|null {
    let selectedLanguage = settings.value.transcriptionLanguage;
    if (selectedLanguage !== null &&
        this.getSodaState(selectedLanguage).value.kind === 'unavailable') {
      // Unselect the language if the selected language pack is unavailable.
      selectedLanguage = null;
    }
    if (selectedLanguage === null && !this.isMultipleLanguageAvailable()) {
      // Use the default language (en-us) when there's no multiple language
      // pack available. Note that the language state may be unavailable.
      selectedLanguage = LanguageCode.EN_US;
    }
    return selectedLanguage;
  }

  /**
   * Returns information of the selected language.
   *
   * Returns null when no language is selected.
   */
  getSelectedLangPackInfo(): LangPackInfo|null {
    const selectedLanguage = this.getSelectedLanguage();
    return selectedLanguage === null ? null :
                                       this.getLangPackInfo(selectedLanguage);
  }

  /**
   * Returns the SODA installation state of the selected language.
   *
   * Returns null when there are multiple languages available, and no language
   * is selected.
   */
  getSelectedLanguageState(): ReadonlySignal<ModelState>|null {
    const selectedLanguage = this.getSelectedLanguage();
    return selectedLanguage === null ? null :
                                       this.getSodaState(selectedLanguage);
  }

  /**
   * Returns whether there are multiple languages available.
   */
  abstract isMultipleLanguageAvailable(): boolean;

  /**
   * Requests installation of SODA library and language pack of given language.
   *
   * Installation state and error will be reported through the `sodaState`.
   */
  abstract installSoda(language: LanguageCode): Promise<void>;

  /**
   * Returns whether SODA is available on the device.
   */
  abstract isSodaAvailable(): boolean;

  /**
   * Returns the SODA installation state of given language.
   */
  abstract getSodaState(language: LanguageCode): ReadonlySignal<ModelState>;

  /**
   * Creates a new soda session for transcription using given language.
   */
  abstract newSodaSession(language: LanguageCode): Promise<SodaSession>;

  /**
   * Returns the additional microphone info of a mic with |deviceId|.
   */
  abstract getMicrophoneInfo(deviceId: string): Promise<InternalMicInfo>;

  /**
   * Renders the UI needed on the dev page.
   */
  abstract renderDevUi(): RenderResult;

  /**
   * Handles an uncaught error.
   */
  abstract handleUncaughtError(error: unknown): void;

  /**
   * Shows feedback dialog for AI with the given description pre-filled.
   */
  abstract showAiFeedbackDialog(description: string): void;

  /**
   * Returns MediaStream capturing the system audio.
   */
  abstract getSystemAudioMediaStream(): Promise<MediaStream>;

  /**
   * Returns the locale used to format various date/time string.
   *
   * The locale returned should be in the format that is compatible with the
   * locales argument for `Intl`.
   * (https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl#locales_argument).
   *
   * Returns undefined to use the system locale from Chrome.
   */
  getLocale(): Intl.LocalesArgument {
    return undefined;
  }

  /**
   * Gets/sets the quiet mode of the system.
   */
  abstract readonly quietMode: Signal<boolean>;

  /**
   * Whether speaker label can be used by current profile.
   *
   * In additional to this, SODA still needs to be supported and installed, and
   * the language pack needs to support speaker label for speaker label to work.
   *
   * Note that in typical SWA case, this value is set and fixed on startup, and
   * currently there's no case where this would change at runtime, but to
   * support easier development we still use a signal here.
   */
  abstract readonly canUseSpeakerLabel: ReadonlySignal<boolean>;

  /**
   * Records a consent for speaker label.
   *
   * Note that there's a legal implication to have the logged strings same as
   * what the user sees, so it should be passed down from close to where the UI
   * is shown, and shouldn't be simply "hard-coded".
   *
   * @param consentGiven Whether the consent is given or not given.
   * @param consentDescriptionNames The list of "string names" (as in the key of
   *     the i18n object) in the consent dialog description.
   * @param consentConfirmationName The "string name" of the consent dialog
   *     confirm button that the user clicked.
   */
  abstract recordSpeakerLabelConsent(
    consentGiven: boolean,
    consentDescriptionNames: NoArgStringName[],
    consentConfirmationName: NoArgStringName,
  ): void;

  /**
   * Whether getDisplayMedia can be used to include system audio.
   *
   * In typical SWA case, this value is set on startup and fixed at runtime, but
   * to support easier development we still use a signal here.
   */
  abstract readonly canCaptureSystemAudioWithLoopback: ReadonlySignal<boolean>;

  /*
   * Events sender to collect events of interest.
   */
  abstract readonly eventsSender: EventsSender;

  /**
   * Performance logger to measure performance.
   */
  abstract readonly perfLogger: PerfLogger;

  /**
   * Adds model state watchers for perf events.
   */
  initPerfEventWatchers(): void {
    // Watcher for summarization model download.
    effect(() => {
      const state = this.summaryModelLoader.state.value;
      const summaryEventType = 'summaryModelDownload';
      if (state.kind === 'installed') {
        // Records perf event only if the download has been initiated from UI.
        this.perfLogger.tryFinish(summaryEventType);
      }
    });

    // Watchers for transcription model download.
    const languageList = this.getLangPackList();
    for (const language of languageList) {
      effect(() => {
        const state = this.getSodaState(language.languageCode).value;
        if (state.kind === 'installed') {
          // Records perf event only if the download has been initiated from UI.
          this.perfLogger.tryFinish(
            createTranscriptionModelDownloadPerf(language.languageCode).kind,
          );
        }
      });
    }
  }
}
