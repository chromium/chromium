// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.speechRecognitionPrivate API
 * Generated from: chrome/common/extensions/api/speech_recognition_private.idl
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/speech_recognition_private.idl -g
 * ts_definitions` to regenerate.
 */

import {ChromeEvent} from '../../../../../../tools/typescript/definitions/chrome_event.js';

declare global {
  export namespace chrome {

    export namespace speechRecognitionPrivate {

      export enum SpeechRecognitionType {
        ON_DEVICE = 'onDevice',
        NETWORK = 'network',
      }

      export interface SpeechRecognitionStopEvent {
        clientId?: number;
      }

      export interface SpeechRecognitionResultEvent {
        clientId?: number;
        transcript: string;
        isFinal: boolean;
      }

      export interface SpeechRecognitionErrorEvent {
        clientId?: number;
        message: string;
      }

      export interface StartOptions {
        clientId?: number;
        locale?: string;
        interimResults?: boolean;
      }

      export interface StopOptions {
        clientId?: number;
      }

      export function start(
          options: StartOptions,
          callback: (type: SpeechRecognitionType) => void): void;

      export function stop(options: StopOptions, callback: () => void): void;

      export const onStop:
          ChromeEvent<(event: SpeechRecognitionStopEvent) => void>;

      export const onResult:
          ChromeEvent<(event: SpeechRecognitionResultEvent) => void>;

      export const onError:
          ChromeEvent<(event: SpeechRecognitionErrorEvent) => void>;

    }
  }
}
