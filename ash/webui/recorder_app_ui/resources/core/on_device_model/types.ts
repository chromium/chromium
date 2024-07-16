// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * All possible model IDs.
 *
 * TODO(pihsun): Abstract the "uuid" part of model, so the real uuid can be put
 * into platform/swa.
 */
export enum ModelId {
  GEMINI_XXS_IT_BASE = 'ee7c31c2-18e5-405a-b54e-f2607130a15d',
  SUMMARY = '73caa678-45cb-4007-abb9-f04e431376da',
}

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
