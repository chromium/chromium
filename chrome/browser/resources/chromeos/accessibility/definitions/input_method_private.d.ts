// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.inputMethodPrivate API
 * Generated from: chrome/common/extensions/api/input_method_private.json
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/input_method_private.json -g ts_definitions` to
 * regenerate.
 */

import {ChromeEvent} from '../../../../../../tools/typescript/definitions/chrome_event.js';

declare global {
  export namespace chrome {

    export namespace inputMethodPrivate {

      export enum MenuItemStyle {
        CHECK = 'check',
        RADIO = 'radio',
        SEPARATOR = 'separator',
      }

      export interface MenuItem {
        id: string;
        label?: string;
        style?: MenuItemStyle;
        visible?: boolean;
        checked?: boolean;
        enabled?: boolean;
      }

      export enum UnderlineStyle {
        UNDERLINE = 'underline',
        DOUBLE_UNDERLINE = 'doubleUnderline',
        NO_UNDERLINE = 'noUnderline',
      }

      export enum FocusReason {
        MOUSE = 'mouse',
        TOUCH = 'touch',
        PEN = 'pen',
        OTHER = 'other',
      }

      export enum InputModeType {
        NO_KEYBOARD = 'noKeyboard',
        TEXT = 'text',
        TEL = 'tel',
        URL = 'url',
        EMAIL = 'email',
        NUMERIC = 'numeric',
        DECIMAL = 'decimal',
        SEARCH = 'search',
      }

      export enum InputContextType {
        TEXT = 'text',
        SEARCH = 'search',
        TEL = 'tel',
        URL = 'url',
        EMAIL = 'email',
        NUMBER = 'number',
        PASSWORD = 'password',
        NULL = 'null',
      }

      export enum AutoCapitalizeType {
        OFF = 'off',
        CHARACTERS = 'characters',
        WORDS = 'words',
        SENTENCES = 'sentences',
      }

      export enum LanguagePackStatus {
        UNKNOWN = 'unknown',
        NOT_INSTALLED = 'notInstalled',
        IN_PROGRESS = 'inProgress',
        INSTALLED = 'installed',
        ERROR_OTHER = 'errorOther',
        ERROR_NEEDS_REBOOT = 'errorNeedsReboot',
      }

      export interface LanguagePackStatusChange {
        engineIds: string[];
        status: LanguagePackStatus;
      }

      export interface InputContext {
        contextID: number;
        type: InputContextType;
        mode: InputModeType;
        autoCorrect: boolean;
        autoComplete: boolean;
        autoCapitalize: AutoCapitalizeType;
        spellCheck: boolean;
        shouldDoLearning: boolean;
        focusReason: FocusReason;
        appKey?: string;
      }

      export interface InputMethodSettings {
        enableCompletion?: boolean;
        enableDoubleSpacePeriod?: boolean;
        enableGestureTyping?: boolean;
        enablePrediction?: boolean;
        enableSoundOnKeypress?: boolean;
        physicalKeyboardAutoCorrectionEnabledByDefault?: boolean;
        physicalKeyboardAutoCorrectionLevel?: number;
        physicalKeyboardEnableCapitalization?: boolean;
        physicalKeyboardEnableDiacriticsOnLongpress?: boolean;
        physicalKeyboardEnablePredictiveWriting?: boolean;
        virtualKeyboardAutoCorrectionLevel?: number;
        virtualKeyboardEnableCapitalization?: boolean;
        xkbLayout?: string;
        koreanEnableSyllableInput?: boolean;
        koreanKeyboardLayout?: string;
        koreanShowHangulCandidate?: boolean;
        pinyinChinesePunctuation?: boolean;
        pinyinDefaultChinese?: boolean;
        pinyinEnableFuzzy?: boolean;
        pinyinEnableLowerPaging?: boolean;
        pinyinEnableUpperPaging?: boolean;
        pinyinFullWidthCharacter?: boolean;
        pinyinFuzzyConfig?: {
          an_ang?: boolean,
          c_ch?: boolean,
          en_eng?: boolean,
          f_h?: boolean,
          ian_iang?: boolean,
          in_ing?: boolean,
          k_g?: boolean,
          l_n?: boolean,
          r_l?: boolean,
          s_sh?: boolean,
          uan_uang?: boolean,
          z_zh?: boolean,
        };
        zhuyinKeyboardLayout?: string;
        zhuyinPageSize?: number;
        zhuyinSelectKeys?: string;
        vietnameseVniAllowFlexibleDiacritics?: boolean;
        vietnameseVniNewStyleToneMarkPlacement?: boolean;
        vietnameseVniInsertDoubleHornOnUo?: boolean;
        vietnameseVniShowUnderline?: boolean;
        vietnameseTelexAllowFlexibleDiacritics?: boolean;
        vietnameseTelexNewStyleToneMarkPlacement?: boolean;
        vietnameseTelexInsertDoubleHornOnUo?: boolean;
        vietnameseTelexInsertUHornOnW?: boolean;
        vietnameseTelexShowUnderline?: boolean;
      }

      export function getInputMethodConfig(
          callback: (config: {
            isPhysicalKeyboardAutocorrectEnabled: boolean,
            isImeMenuActivated: boolean,
          }) => void): void;

      export function getInputMethods(callback: (methods: Array<{
                                        id: string,
                                        name: string,
                                        indicator: string,
                                      }>) => void): void;

      export function getCurrentInputMethod(callback: (method: string) => void):
          void;

      export function setCurrentInputMethod(
          inputMethodId: string, callback?: () => void): void;

      export function switchToLastUsedInputMethod(callback?: () => void): void;

      export function fetchAllDictionaryWords(
          callback: (words: string[]) => void): void;

      export function addWordToDictionary(word: string, callback?: () => void):
          void;

      /* eslint-disable-next-line @typescript-eslint/naming-convention */
      export function setXkbLayout(xkb_name: string, callback?: () => void):
          void;

      export function finishComposingText(
          parameters: {
            contextID: number,
          },
          callback?: () => void): void;

      export function showInputView(callback?: () => void): void;

      export function hideInputView(callback?: () => void): void;

      export function openOptionsPage(inputMethodId: string): void;

      export function getSurroundingText(
          beforeLength: number, afterLength: number, callback: (result: {
                                                       before: string,
                                                       selected: string,
                                                       after: string,
                                                     }) => void): void;

      export function getSettings(
          engineID: string,
          callback: (settings: InputMethodSettings) => void): void;

      export function setSettings(
          engineID: string, settings: InputMethodSettings,
          callback?: () => void): void;

      export function setCompositionRange(
          parameters: {
            contextID: number,
            selectionBefore: number,
            selectionAfter: number,
            segments?: Array<{
                      start: number,
                      end: number,
                      style: UnderlineStyle,
                    }>,
          },
          callback?: (accepted: boolean) => void): void;

      export function reset(): void;

      export function onAutocorrect(parameters: {
        contextID: number,
        typedWord: string,
        correctedWord: string,
        startIndex: number,
      }): void;

      export function notifyInputMethodReadyForTesting(): void;

      export function getLanguagePackStatus(
          inputMethodId: string,
          callback: (status: LanguagePackStatus) => void): void;

      export const onCaretBoundsChanged: ChromeEvent<(caretBounds: {
                                                       x: number,
                                                       y: number,
                                                       w: number,
                                                       h: number,
                                                     }) => void>;

      export const onChanged: ChromeEvent<(newInputMethodId: string) => void>;

      export const onDictionaryLoaded: ChromeEvent<() => void>;

      export const onDictionaryChanged:
          ChromeEvent<(added: string[], removed: string[]) => void>;

      export const onImeMenuActivationChanged:
          ChromeEvent<(activation: boolean) => void>;

      export const onImeMenuListChanged: ChromeEvent<() => void>;

      export const onImeMenuItemsChanged:
          ChromeEvent<(engineID: string, items: MenuItem[]) => void>;

      export const onFocus: ChromeEvent<(context: InputContext) => void>;

      export const onSettingsChanged: ChromeEvent<
          (engineID: string, settings: InputMethodSettings) => void>;

      export const onScreenProjectionChanged:
          ChromeEvent<(isProjected: boolean) => void>;

      export const onSuggestionsChanged:
          ChromeEvent<(suggestions: string[]) => void>;

      export const onInputMethodOptionsChanged:
          ChromeEvent<(engineID: string) => void>;

      export const onLanguagePackStatusChanged:
          ChromeEvent<(change: LanguagePackStatusChange) => void>;

    }
  }
}
