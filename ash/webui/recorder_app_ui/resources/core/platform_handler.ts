// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InternalMicInfo} from './microphone_manager.js';
import {Model, ModelId, ModelState} from './on_device_model/types.js';
import {ReadonlySignal} from './reactive/signal.js';
import {SodaSession} from './soda/types.js';
import {assertExists} from './utils/assert.js';

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
}
