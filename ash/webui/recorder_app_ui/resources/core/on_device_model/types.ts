// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ReadonlySignal} from '../reactive/signal.js';

/**
 * Model installation state.
 *
 * This is also used by SODA.
 */
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

/**
 * Possible error types from model responses.
 */
export enum ModelResponseError {
  // General error.
  GENERAL = 'GENERAL',

  // The transcription word count is less than the minimum length.
  UNSUPPORTED_TRANSCRIPTION_IS_TOO_SHORT =
    'UNSUPPORTED_TRANSCRIPTION_IS_TOO_SHORT',

  // The transcription word count is more than the maximum length.
  UNSUPPORTED_TRANSCRIPTION_IS_TOO_LONG =
    'UNSUPPORTED_TRANSCRIPTION_IS_TOO_LONG',

  // Filtered by T&S on the request or response string.
  UNSAFE = 'UNSAFE',
}

export enum GenaiResultType {
  SUMMARY = 'SUMMARY',
  TITLE_SUGGESTION = 'TITLE_SUGGESTION',
}

// prettier-ignore
export type ModelResponse<T> = {
  kind: 'error',
  error: ModelResponseError,
}|{
  kind: 'success',
  result: T,
};

export abstract class ModelLoader<T> {
  /**
   * The state of the model.
   */
  abstract state: ReadonlySignal<ModelState>;

  /**
   * Loads the model.
   *
   * TODO(pihsun): Currently no usage reuse loaded models, so the
   * `loadAndExecute` API is sufficient. Define proper "stages" for the model
   * API when there's need to reuse the loaded model.
   *
   * TODO(pihsun): Returns specific error type when the model failed to load.
   */
  protected abstract load(): Promise<Model<T>|null>;

  /**
   * Loads the model and execute it on an input.
   */
  abstract loadAndExecute(content: string): Promise<ModelResponse<T>>;

  /**
   * Requests download of the given model.
   */
  download(): void {
    // TODO(pihsun): There's currently no way of requesting download of the
    // model but not load it, so we load the model (which downloads the model)
    // and then immediately unloads it. Check the performance overhead and
    // consider adding another API for only downloading the model if the
    // overhead is large.
    void this.load().then((model) => {
      model?.close();
    });
  }
}

export interface Model<T> {
  /**
   * Returns the model response based on content.
   */
  execute(content: string): Promise<ModelResponse<T>>;

  /**
   * Closes the model connection.
   *
   * This should release resources used by the model, and no further call of
   * other calls should happen after this.
   */
  close(): void;
}
