// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventsSender} from './events_sender.js';
import {NoArgStringName} from './i18n.js';
import {InternalMicInfo} from './microphone_manager.js';
import {ModelLoader, ModelState} from './on_device_model/types.js';
import {PerfLogger} from './perf.js';
import {ReadonlySignal, Signal} from './reactive/signal.js';
import {SodaSession} from './soda/types.js';

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
   * Requests installation of SODA library and language pack.
   *
   * Installation state and error will be reported through the `sodaState`.
   */
  abstract installSoda(): void;

  /**
   * The SODA installation state.
   */
  abstract readonly sodaState: ReadonlySignal<ModelState>;

  /**
   * Creates a new soda session for transcription.
   */
  abstract newSodaSession(): Promise<SodaSession>;

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
}
