// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.input.ime API
 * Generated from: chrome/common/extensions/api/input_ime.json
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/input_ime.json -g ts_definitions` to regenerate.
 */

import {ChromeEvent} from '../../../../../../tools/typescript/definitions/chrome_event.js';

declare global {
  export namespace chrome {

    export namespace input.ime {

      export enum KeyboardEventType {
        KEYUP = 'keyup',
        KEYDOWN = 'keydown',
      }

      export interface KeyboardEvent {
        type: KeyboardEventType;
        requestId?: string;
        extensionId?: string;
        key: string;
        code: string;
        keyCode?: number;
        altKey?: boolean;
        altgrKey?: boolean;
        ctrlKey?: boolean;
        shiftKey?: boolean;
        capsLock?: boolean;
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
        CHARACTERS = 'characters',
        WORDS = 'words',
        SENTENCES = 'sentences',
      }

      export interface InputContext {
        contextID: number;
        type: InputContextType;
        autoCorrect: boolean;
        autoComplete: boolean;
        autoCapitalize: AutoCapitalizeType;
        spellCheck: boolean;
        shouldDoLearning: boolean;
      }

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

      export enum WindowPosition {
        CURSOR = 'cursor',
        COMPOSITION = 'composition',
      }

      export enum ScreenType {
        NORMAL = 'normal',
        LOGIN = 'login',
        LOCK = 'lock',
        SECONDARY_LOGIN = 'secondary-login',
      }

      export enum MouseButton {
        LEFT = 'left',
        MIDDLE = 'middle',
        RIGHT = 'right',
      }

      export enum AssistiveWindowType {
        UNDO = 'undo',
      }

      export interface AssistiveWindowProperties {
        type: AssistiveWindowType;
        visible: boolean;
        announceString?: string;
      }

      export enum AssistiveWindowButton {
        UNDO = 'undo',
        ADD_TO_DICTIONARY = 'addToDictionary',
      }

      export interface MenuParameters {
        engineID: string;
        items: MenuItem[];
      }

      export function setComposition(
          parameters: {
            contextID: number,
            text: string,
            selectionStart?: number,
            selectionEnd?: number, cursor: number,
            segments?: Array<{
                      start: number,
                      end: number,
                      style: UnderlineStyle,
                    }>,
          },
          callback?: (success: boolean) => void): void;

      export function clearComposition(
          parameters: {
            contextID: number,
          },
          callback?: (success: boolean) => void): void;

      export function commitText(
          parameters: {
            contextID: number,
            text: string,
          },
          callback?: (success: boolean) => void): void;

      export function sendKeyEvents(
          parameters: {
            contextID: number,
            keyData: KeyboardEvent[],
          },
          callback?: () => void): void;

      export function hideInputView(): void;

      export function setCandidateWindowProperties(
          parameters: {
            engineID: string,
            properties: {
              visible?: boolean,
              cursorVisible?: boolean,
              vertical?: boolean,
              pageSize?: number,
              auxiliaryText?: string,
              auxiliaryTextVisible?: boolean,
              totalCandidates?: number,
              currentCandidateIndex?: number,
              windowPosition?: WindowPosition,
            },
          },
          callback?: (success: boolean) => void): void;

      export function setCandidates(
          parameters: {
            contextID: number,
            candidates: Array<{
              candidate: string,
              id: number,
              parentId?: number,
              label?: string,
              annotation?: string,
              usage?: {
                title: string,
                body: string,
              },
            }>,
          },
          callback?: (success: boolean) => void): void;

      export function setCursorPosition(
          parameters: {
            contextID: number,
            candidateID: number,
          },
          callback?: (success: boolean) => void): void;

      export function setAssistiveWindowProperties(
          parameters: {
            contextID: number,
            properties: AssistiveWindowProperties,
          },
          callback?: (success: boolean) => void): void;

      export function setAssistiveWindowButtonHighlighted(
          parameters: {
            contextID: number,
            buttonID: AssistiveWindowButton,
            windowType: AssistiveWindowType,
            announceString?: string, highlighted: boolean,
          },
          callback?: () => void): void;

      export function setMenuItems(
          parameters: MenuParameters, callback?: () => void): void;

      export function updateMenuItems(
          parameters: MenuParameters, callback?: () => void): void;

      export function deleteSurroundingText(
          parameters: {
            engineID: string,
            contextID: number,
            offset: number,
            length: number,
          },
          callback?: () => void): void;

      export function keyEventHandled(requestId: string, response: boolean):
          void;

      export const onActivate:
          ChromeEvent<(engineID: string, screen: ScreenType) => void>;

      export const onDeactivated: ChromeEvent<(engineID: string) => void>;

      export const onFocus: ChromeEvent<(context: InputContext) => void>;

      export const onBlur: ChromeEvent<(contextID: number) => void>;

      export const onInputContextUpdate:
          ChromeEvent<(context: InputContext) => void>;

      export const onKeyEvent: ChromeEvent<
          (engineID: string, keyData: KeyboardEvent, requestId: string) =>
              boolean>;

      export const onCandidateClicked: ChromeEvent<
          (engineID: string, candidateID: number, button: MouseButton) => void>;

      export const onMenuItemActivated:
          ChromeEvent<(engineID: string, name: string) => void>;

      export const onSurroundingTextChanged:
          ChromeEvent<(engineID: string, surroundingInfo: {
                        text: string,
                        focus: number,
                        anchor: number,
                        offset: number,
                      }) => void>;

      export const onReset: ChromeEvent<(engineID: string) => void>;

      export const onAssistiveWindowButtonClicked:
          ChromeEvent<(details: {
                        buttonID: AssistiveWindowButton,
                        windowType: AssistiveWindowType,
                      }) => void>;

    }
  }
}
