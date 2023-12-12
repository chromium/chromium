// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.languageSettingsPrivate API
 * Generated from: chrome/common/extensions/api/language_settings_private.idl
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/language_settings_private.idl -g ts_definitions`
 * to regenerate.
 */

import {ChromeEvent} from '../../../../../../tools/typescript/definitions/chrome_event.js';

declare global {
  export namespace chrome {

    export namespace languageSettingsPrivate {

      export enum MoveType {
        TOP = 'TOP',
        UP = 'UP',
        DOWN = 'DOWN',
        UNKNOWN = 'UNKNOWN',
      }

      export interface Language {
        code: string;
        displayName: string;
        nativeDisplayName: string;
        supportsUI?: boolean;
        supportsSpellcheck?: boolean;
        supportsTranslate?: boolean;
        isProhibitedLanguage?: boolean;
      }

      export interface SpellcheckDictionaryStatus {
        languageCode: string;
        isReady: boolean;
        isDownloading?: boolean;
        downloadFailed?: boolean;
      }

      export interface InputMethod {
        id: string;
        displayName: string;
        languageCodes: string[];
        tags: string[];
        enabled?: boolean;
        hasOptionsPage?: boolean;
        isProhibitedByPolicy?: boolean;
      }

      export interface InputMethodLists {
        componentExtensionImes: InputMethod[];
        thirdPartyExtensionImes: InputMethod[];
      }

      export function getLanguageList(
          callback: (languages: Language[]) => void): void;

      export function enableLanguage(languageCode: string): void;

      export function disableLanguage(languageCode: string): void;

      export function setEnableTranslationForLanguage(
          languageCode: string, enable: boolean): void;

      export function moveLanguage(languageCode: string, moveType: MoveType):
          void;

      export function getAlwaysTranslateLanguages(
          callback: (languages: string[]) => void): void;

      export function setLanguageAlwaysTranslateState(
          languageCode: string, alwaysTranslate: boolean): void;

      export function getNeverTranslateLanguages(
          callback: (languages: string[]) => void): void;

      export function getSpellcheckDictionaryStatuses(
          callback: (status: SpellcheckDictionaryStatus[]) => void): void;

      export function getSpellcheckWords(callback: (words: string[]) => void):
          void;

      export function addSpellcheckWord(word: string): void;

      export function removeSpellcheckWord(word: string): void;

      export function getTranslateTargetLanguage(
          callback: (language: string) => void): void;

      export function setTranslateTargetLanguage(languageCode: string): void;

      export function getInputMethodLists(
          callback: (methods: InputMethodLists) => void): void;

      export function addInputMethod(inputMethodId: string): void;

      export function removeInputMethod(inputMethodId: string): void;

      export function retryDownloadDictionary(languageCode: string): void;

      export const onSpellcheckDictionariesChanged:
          ChromeEvent<(statuses: SpellcheckDictionaryStatus[]) => void>;

      export const onCustomDictionaryChanged:
          ChromeEvent<(wordsAdded: string[], wordsRemoved: string[]) => void>;

      export const onInputMethodAdded:
          ChromeEvent<(inputMethodId: string) => void>;

      export const onInputMethodRemoved:
          ChromeEvent<(inputMethodId: string) => void>;

    }
  }
}
