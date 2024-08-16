// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InternalMicInfo} from './microphone_manager.js';
import {ModelLoader, ModelState} from './on_device_model/types.js';
import {ReadonlySignal, Signal} from './reactive/signal.js';
import {SodaSession} from './soda/types.js';

export abstract class PlatformHandler {
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
   * Returns the formatted localized string by given `id` and `args`.
   *
   * This is the lower level function that is used to implement the `i18n`
   * helper in core/i18n.ts, and shouldn't be directly used.
   * The `i18n` helper provides better typing and should be used instead.
   */
  abstract getStringF(id: string, ...args: Array<number|string>): string;

  /**
   * Renders the UI needed on the dev page.
   */
  abstract renderDevUi(): RenderResult;

  /**
   * Handles an uncaught error and returns the error UI to be shown.
   *
   * Returns null if the error is not handled specifically by the platform
   * handler.
   */
  abstract handleUncaughtError(error: unknown): RenderResult|null;

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
}
