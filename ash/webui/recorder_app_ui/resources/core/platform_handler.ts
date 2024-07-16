// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ReadonlySignal} from './reactive/signal.js';
import {SodaEvent} from './soda/types.js';
import {assertExists} from './utils/assert.js';
import {Observer, Unsubscribe} from './utils/observer_list.js';

export enum ModelId {
  // TODO(pihsun): Abstract the "uuid" part of model.
  GEMINI_XXS_IT_BASE = 'ee7c31c2-18e5-405a-b54e-f2607130a15d',
  SUMMARY = '73caa678-45cb-4007-abb9-f04e431376da',
}

// prettier-ignore
export type ModelState = {
  kind: 'error'|'installed'|'notInstalled'|'unavailable',
}|{
  kind: 'installing',

  /**
   * A number between 0 to 100 indicating the progress of the download / install
   * of SODA or on-device model.
   */
  progress: number,
};

export enum ModelResponseError {
  // General error.
  GENERAL = 'GENERAL',

  // Filtered by T&S on the request or response string.
  UNSAFE = 'UNSAFE',
}

// prettier-ignore
export type ModelResponse<T = string> = {
  kind: 'error',
  error: ModelResponseError,
}|{
  kind: 'success',
  result: T,
};

export interface Model {
  /**
   * Returns the suggested titles based on content.
   */
  // TODO(pihsun): method to set input options.
  suggestTitles(content: string): Promise<ModelResponse<string[]>>;

  /**
   * Generates a short summarization of the given content.
   */
  summarize(content: string): Promise<ModelResponse<string>>;

  /**
   * Closes the model connection.
   *
   * This should release resources used by the model, and no further call of
   * other calls should happen after this.
   */
  close(): void;
}

export interface SodaSession {
  /**
   * Starts the session.
   *
   * Each session can be started at most once.
   */
  start(): Promise<void>;

  /**
   * Adds audio sample to the session.
   *
   * `start` must be called before this.
   *
   * @param samples Array of sample of the audio. Each sample value is in the
   *     range of [-1, 1].
   */
  addAudio(samples: Float32Array): void;

  /**
   * Stops the session and waits for all audio samples to be processed.
   *
   * This can only be called at most once, and `start` must be called before
   * this.
   */
  stop(): Promise<void>;

  /**
   * Add an observer for the result SodaEvent.
   *
   * Since events before subscriptions are not passed to observer on subscribe,
   * this should be called before audio samples are added to ensure that no
   * event is dropped.
   */
  subscribeEvent(observer: Observer<SodaEvent>): Unsubscribe;
}

export abstract class PlatformHandler {
  /**
   * Initializes the platform handler.
   *
   * This should only be called once when the app starts.
   */
  abstract init(): Promise<void>;

  /**
   * Maps from model ID to the ML model by the given ID installation state.
   *
   * The key should contain all member of all `ModelId`.
   */
  protected abstract readonly modelStates:
    Map<ModelId, ReadonlySignal<ModelState>>;

  getModelState(modelId: ModelId): ReadonlySignal<ModelState> {
    return assertExists(this.modelStates.get(modelId));
  }

  /**
   * Loads the model by the given model ID.
   */
  abstract loadModel(modelId: ModelId): Promise<Model>;

  /**
   * Requests download of the given model.
   */
  downloadModel(modelId: ModelId): void {
    // TODO(pihsun): There's currently no way of requesting download of the
    // model but not load it, so we load the model (which downloads the model)
    // and then immediately unloads it. Check the performance overhead and
    // consider adding another API for only downloading the model if the
    // overhead is large.
    void this.loadModel(modelId).then((model) => {
      model.close();
    });
  }

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
}
