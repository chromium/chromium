// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.ttsEngine API
 * Generated from: chrome/common/extensions/api/tts_engine.json
 * run `tools/json_schema_compiler/compiler.py
 * chrome/common/extensions/api/tts_engine.json -g ts_definitions` to
 * regenerate.
 */

import {ChromeEvent} from '../../../../../../tools/typescript/definitions/chrome_event.js';

declare global {
  export namespace chrome {

    export namespace ttsEngine {

      export enum VoiceGender {
        MALE = 'male',
        FEMALE = 'female',
      }

      export interface SpeakOptions {
        voiceName?: string;
        lang?: string;
        gender?: VoiceGender;
        rate?: number;
        pitch?: number;
        volume?: number;
      }

      export interface AudioStreamOptions {
        sampleRate: number;
        bufferSize: number;
      }

      export interface AudioBuffer {
        audioBuffer: ArrayBuffer;
        charIndex?: number;
        isLastBuffer?: boolean;
      }

      export function updateVoices(voices: tts.TtsVoice[]): void;

      export function sendTtsEvent(requestId: number, event: tts.TtsEvent):
          void;

      export function sendTtsAudio(requestId: number, audio: AudioBuffer): void;

      export const onSpeak: ChromeEvent<
          (utterance: string, options: SpeakOptions,
           sendTtsEvent: (event: tts.TtsEvent) => void) => void>;

      export const onSpeakWithAudioStream: ChromeEvent<
          (utterance: string, options: SpeakOptions,
           audioStreamOptions: AudioStreamOptions,
           sendTtsAudio: (audioBufferParams: AudioBuffer) => void,
           sendError: (errorMessage: string) => void) => void>;

      export const onStop: ChromeEvent<() => void>;

      export const onPause: ChromeEvent<() => void>;

      export const onResume: ChromeEvent<() => void>;

    }
  }
}
