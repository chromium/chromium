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

import type {ChromeEvent} from '../../../../../../tools/typescript/definitions/chrome_event.js';

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

      export enum JapaneseInputMode {
        KANA = 'Kana',
        ROMAJI = 'Romaji',
      }

      export enum JapanesePunctuationStyle {
        KUTEN_TOUTEN = 'KutenTouten',
        COMMA_PERIOD = 'CommaPeriod',
        KUTEN_PERIOD = 'KutenPeriod',
        COMMA_TOUTEN = 'CommaTouten',
      }

      export enum JapaneseSymbolStyle {
        CORNER_BRACKET_MIDDLE_DOT = 'CornerBracketMiddleDot',
        SQUARE_BRACKET_SLASH = 'SquareBracketSlash',
        CORNER_BRACKET_SLASH = 'CornerBracketSlash',
        SQUARE_BRACKET_MIDDLE_DOT = 'SquareBracketMiddleDot',
      }

      export enum JapaneseSpaceInputStyle {
        INPUT_MODE = 'InputMode',
        FULLWIDTH = 'Fullwidth',
        HALFWIDTH = 'Halfwidth',
      }

      export enum JapaneseSelectionShortcut {
        NO_SHORTCUT = 'NoShortcut',
        DIGITS123456789 = 'Digits123456789',
        ASDFGHJKL = 'ASDFGHJKL',
      }

      export enum JapaneseKeymapStyle {
        ATOK = 'Atok',
        MS_IME = 'MsIme',
        KOTOERI = 'Kotoeri',
        CHROME_OS = 'ChromeOs',
      }

      export enum ShiftKeyModeStyle {
        OFF = 'Off',
        ALPHANUMERIC = 'Alphanumeric',
        KATAKANA = 'Katakana',
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
        JapaneseInputMode?: JapaneseInputMode;
        JapanesePunctuationStyle?: JapanesePunctuationStyle;
        JapaneseSymbolStyle?: JapaneseSymbolStyle;
        JapaneseSpaceInputStyle?: JapaneseSpaceInputStyle;
        JapaneseSectionShortcut?: JapaneseSelectionShortcut;
        JapaneseKeymapStyle?: JapaneseKeymapStyle;
        AutomaticallySwitchToHalfwidth?: boolean;
        ShiftKeyModeStyle?: ShiftKeyModeStyle;
        UseInputHistory?: boolean;
        UseSystemDictionary?: boolean;
        numberOfSuggestions?: number;
        JapaneseDisableSuggestions?: boolean;
        koreanEnableSyllableInput?: boolean;
        koreanKeyboardLayout?: string;
        koreanShowHangulCandidate?: boolean;
        pinyinChinesePunctuation?: boolean;
        pinyinDefaultChinese?: boolean;
        pinyinEnableLowerPaging?: boolean;
        pinyinEnableUpperPaging?: boolean;
        pinyinFullWidthCharacter?: boolean;
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

      export function getInputMethodConfig(): Promise<{
        isPhysicalKeyboardAutocorrectEnabled: boolean,
        isImeMenuActivated: boolean,
      }>;

      export function getInputMethods(): Promise<Array<{
        id: string,
        name: string,
        indicator: string,
      }>>;

      export function getCurrentInputMethod(): Promise<string>;

      export function setCurrentInputMethod(inputMethodId: string):
          Promise<void>;

      export function switchToLastUsedInputMethod(): Promise<void>;

      export function fetchAllDictionaryWords(): Promise<string[]>;

      export function addWordToDictionary(word: string): Promise<void>;

      export function setXkbLayout(xkb_name: string): Promise<void>;

      export function finishComposingText(parameters: {
        contextID: number,
      }): Promise<void>;

      export function showInputView(): Promise<void>;

      export function hideInputView(): Promise<void>;

      export function openOptionsPage(inputMethodId: string): void;

      export function getSurroundingText(
          beforeLength: number, afterLength: number): Promise<{
        before: string,
        selected: string,
        after: string,
      }>;

      export function getSettings(engineID: string):
          Promise<InputMethodSettings|undefined>;

      export function setSettings(
          engineID: string, settings: InputMethodSettings): Promise<void>;

      export function setCompositionRange(parameters: {
        contextID: number,
        selectionBefore: number,
        selectionAfter: number,
        segments?: Array<{
                  start: number,
                  end: number,
                  style: UnderlineStyle,
                }>,
      }): Promise<boolean>;

      export function reset(): void;

      export function onAutocorrect(parameters: {
        contextID: number,
        typedWord: string,
        correctedWord: string,
        startIndex: number,
      }): void;

      export function notifyInputMethodReadyForTesting(): void;

      export function getLanguagePackStatus(inputMethodId: string):
          Promise<LanguagePackStatus>;

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
