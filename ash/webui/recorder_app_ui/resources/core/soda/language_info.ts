// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Language code used for transcription.
 *
 * This is temporarily listed since the only supported language is en-US, and
 * should be replaced with the type from `LanguageCode` in
 * components/soda/constants.h.
 */
export enum LanguageCode {
  EN_US = 'en-US',
}

export interface LangPackInfo {
  languageCode: LanguageCode;

  /**
   * Language name displayed in the application locale.
   */
  displayName: string;

  /**
   * Whether summarization and title suggestion support this language.
   */
  isGenAiSupported: boolean;
}
