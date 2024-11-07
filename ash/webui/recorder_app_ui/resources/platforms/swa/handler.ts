// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * strings.m.js is generated when we enable it via UseStringsJs() in webUI
 * controller. When loading it, it will populate data such as localized strings
 * into |loadTimeData| that is imported below.
 */
import '/strings.m.js';

import {
  ColorChangeUpdater,
} from
  'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {nothing} from 'chrome://resources/mwc/lit/index.js';

import {NoArgStringName} from '../../core/i18n.js';
import {InternalMicInfo} from '../../core/microphone_manager.js';
import {ModelState} from '../../core/on_device_model/types.js';
import {PerfLogger} from '../../core/perf.js';
import {
  PlatformHandler as PlatformHandlerBase,
} from '../../core/platform_handler.js';
import {computed, Signal, signal} from '../../core/reactive/signal.js';
import {LangPackInfo, LanguageCode} from '../../core/soda/language_info.js';
import {SodaSession} from '../../core/soda/types.js';
import {settings} from '../../core/state/settings.js';
import {
  assertExists,
  assertInstanceof,
  checkEnumVariant,
} from '../../core/utils/assert.js';
import {parseTopFrameInfo} from '../../core/utils/errors.js';
import {lazyInit} from '../../core/utils/utils.js';

import {EventsSender} from './metrics.js';
import {
  mojoModelStateToModelState,
  SummaryModelLoader,
  TitleSuggestionModelLoader,
} from './on_device_model.js';
import {MojoSodaSession} from './soda_session.js';
import {
  LangPackInfo as MojoLangPackInfo,
  ModelState as MojoModelState,
  ModelStateMonitorReceiver,
  PageHandler as MojoPageHandler,
  QuietModeMonitorReceiver,
  SodaClientReceiver,
  SodaRecognizerRemote,
} from './types.js';

const CRASH_SERVER_PRODUCT_NAME = 'ChromeOS_RecorderApp';

export class PlatformHandler extends PlatformHandlerBase {
  private readonly remote = MojoPageHandler.getRemote();

  private readonly sodaStates = new Map<LanguageCode, Signal<ModelState>>();

  private readonly langPacks = new Map<LanguageCode, LangPackInfo>();

  override summaryModelLoader: SummaryModelLoader;

  override titleSuggestionModelLoader: TitleSuggestionModelLoader;

  readonly quietModeInternal = signal(false);

  override readonly quietMode: Signal<boolean>;

  override canUseSpeakerLabel = signal(false);

  static override getStringF(id: string, ...args: Array<number|string>):
    string {
    return loadTimeData.getStringF(id, ...args);
  }

  override readonly canCaptureSystemAudioWithLoopback = signal(false);

  override readonly eventsSender = new EventsSender();

  override perfLogger = new PerfLogger(this.eventsSender);

  constructor() {
    super();
    this.summaryModelLoader = new SummaryModelLoader(this.remote, this);
    this.titleSuggestionModelLoader = new TitleSuggestionModelLoader(
      this.remote,
      this,
    );
    this.quietMode = computed({
      get: () => {
        return this.quietModeInternal.value;
      },
      set: (quietMode: boolean) => {
        this.remote.setQuietMode(quietMode);
        this.quietModeInternal.value = quietMode;
      },
    });
  }

  private mojoLangPackInfoToLangPackInfo(
    langPack: MojoLangPackInfo,
  ): LangPackInfo|null {
    const languageCode = checkEnumVariant(LanguageCode, langPack.languageCode);
    if (languageCode === null) {
      return null;
    }
    return {
      languageCode: languageCode,
      displayName: mojoString16ToString(langPack.displayName),
      isGenAiSupported: langPack.isGenAiSupported,
      isSpeakerLabelSupported: langPack.isSpeakerLabelSupported,
    };
  }

  override async init(): Promise<void> {
    ColorChangeUpdater.forDocument().start();

    this.canUseSpeakerLabel.value =
      (await this.remote.canUseSpeakerLabel()).supported;

    this.canCaptureSystemAudioWithLoopback.value =
      (await this.remote.canCaptureSystemAudioWithLoopback()).supported;

    const mojoLangPacks = (await this.remote.getAvailableLangPacks()).langPacks;
    for (const mojoLangPack of mojoLangPacks) {
      const langPack = this.mojoLangPackInfoToLangPackInfo(mojoLangPack);
      if (langPack === null) {
        continue;
      }
      this.langPacks.set(langPack.languageCode, langPack);
    }

    for (const language of this.langPacks.keys()) {
      const sodaState = signal<ModelState>({kind: 'unavailable'});
      this.sodaStates.set(language, sodaState);
      function update(state: MojoModelState) {
        sodaState.value = mojoModelStateToModelState(state);
      }
      const monitor = new ModelStateMonitorReceiver({update});
      // This should be relatively quick since in recorder_app_ui.cc we just
      // return the cached state here, but we await here to avoid UI showing
      // temporary unavailabe state.
      const {state} = await this.remote.addSodaMonitor(
        language,
        monitor.$.bindNewPipeAndPassRemote(),
      );
      update(state);
    }

    const quietModeMonitor = new QuietModeMonitorReceiver({
      update: (inQuietMode: boolean) => {
        this.quietModeInternal.value = inQuietMode;
      },
    });
    const {inQuietMode} = await this.remote.addQuietModeMonitor(
      quietModeMonitor.$.bindNewPipeAndPassRemote(),
    );
    this.quietModeInternal.value = inQuietMode;

    await this.summaryModelLoader.init();
    await this.titleSuggestionModelLoader.init();
  }

  override getLangPackList = lazyInit((): readonly LangPackInfo[] => {
    return Array.from(this.langPacks.values());
  });

  override getLangPackInfo(language: LanguageCode): LangPackInfo {
    return assertExists(this.langPacks.get(language));
  }

  override async installSoda(language: LanguageCode): Promise<void> {
    // Wait the request goes through to make sure all soda states are updated.
    // The install progress is separately tracked in `sodaState`.
    // TODO: b/375306309 - Remove "await" when soda states are always consistent
    // after the `OnSodaUninstalled` event is implemented.
    await this.remote.installSoda(language);
  }

  override isSodaAvailable(): boolean {
    for (const state of this.sodaStates.values()) {
      if (state.value.kind !== 'unavailable') {
        return true;
      }
    }
    return false;
  }

  override getSodaState(language: LanguageCode): Signal<ModelState> {
    // All language states should be initialized in `init`.
    return assertExists(this.sodaStates.get(language));
  }

  override getSelectedLanguageState(): Signal<ModelState>|null {
    const selectedLanguage = settings.value.transcriptionLanguage;
    return selectedLanguage === null ? null :
                                       this.getSodaState(selectedLanguage);
  }

  override async newSodaSession(
    language: LanguageCode,
  ): Promise<SodaSession> {
    const recognizer = new SodaRecognizerRemote();
    const session = new MojoSodaSession(recognizer);
    const client = new SodaClientReceiver(session);
    const {result} = await this.remote.loadSpeechRecognizer(
      language,
      client.$.bindNewPipeAndPassRemote(),
      recognizer.$.bindNewPipeAndPassReceiver(),
    );
    if (!result) {
      // TODO(pihsun): Dedicated error type?
      throw new Error('Load soda failed');
    }
    return session;
  }

  override async getMicrophoneInfo(deviceId: string): Promise<InternalMicInfo> {
    const info = (await this.remote.getMicrophoneInfo(deviceId)).info;
    // TODO(kamchonlathorn): Consider if it should return the default value or
    // drop the mic from the list instead.
    return info ?? {isDefault: false, isInternal: false};
  }

  override renderDevUi(): RenderResult {
    return nothing;
  }

  override handleUncaughtError(errorRaw: unknown): void {
    const error = assertInstanceof(errorRaw, Error);

    // TODO: b/327538011 - Hook the error handling with the integration tests so
    // that it can properly fail the test when an error is thrown.

    const stackStr = error.stack ?? '';
    const {lineNo, colNo} = parseTopFrameInfo(stackStr);

    chrome.crashReportPrivate.reportError(
      {
        product: CRASH_SERVER_PRODUCT_NAME,
        url: self.location.href,
        message: `${error.name}: ${error.message}`,
        lineNumber: lineNo,
        stackTrace: stackStr,
        columnNumber: colNo,
      },
      /* callback= */
      () => {
        // Do nothing after error reported.
      },
    );
  }

  override showAiFeedbackDialog(description: string): void {
    this.remote.openAiFeedbackDialog(description);
  }

  override async getSystemAudioMediaStream(): Promise<MediaStream> {
    return navigator.mediaDevices.getDisplayMedia({
      // `video: false` can be used here with the special permission
      // DISPLAY_MEDIA_SYSTEM_AUDIO.
      video: false,
      audio: {
        autoGainControl: {ideal: false},
        echoCancellation: {ideal: false},
        noiseSuppression: {ideal: false},
      },
      systemAudio: 'include',
    });
  }

  override recordSpeakerLabelConsent(
    consentGiven: boolean,
    consentDescriptionNames: NoArgStringName[],
    consentConfirmationName: NoArgStringName,
  ): void {
    this.remote.recordSpeakerLabelConsent(
      consentGiven,
      consentDescriptionNames,
      consentConfirmationName,
    );
  }
}
