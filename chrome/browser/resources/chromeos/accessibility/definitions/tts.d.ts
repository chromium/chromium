// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ChromeEvent} from '../../../../../../tools/typescript/definitions/chrome_event.js';

// TODO(crbug.com/40179454): Auto-generate this file.

declare global {
  namespace chrome {
    export namespace tts {

      export enum EventType {
        START = 'start',
        END = 'end',
        WORD = 'word',
        SENTENCE = 'sentence',
        MARKER = 'marker',
        INTERRUPTED = 'interrupted',
        CANCELLED = 'cancelled',
        ERROR = 'error',
        PAUSE = 'pause',
        RESUME = 'resume',
      }

      export class TtsOptions {
        enqueue?: boolean;
        voiceName?: string;
        extensionId?: string;
        lang?: string;
        rate?: number;
        pitch?: number;
        volume?: number;
        requiredEventTypes?: string[];
        desiredEventTypes?: string[];
        onEvent?: (event: TtsEvent) => void;
      }

      export interface TtsEvent {
        type: EventType;
        charIndex?: number;
        errorMessage?: string;
        srcId?: number;
        isFinalEvent?: boolean;
        length?: number;
      }

      export interface TtsVoice {
        voiceName?: string;
        lang?: string;
        remote?: boolean;
        extensionId?: string;
        eventTypes?: EventType[];
      }

      export function speak(
          utterance: string, options: TtsOptions, callback?: () => void): void;

      export function stop(): void;

      export function pause(): void;

      export function resume(): void;

      export function isSpeaking(callback?: (param: boolean) => void): void;

      export function getVoices(callback?: (param: TtsVoice[]) => void): void;

      export const onEvent: ChromeEvent<(event: TtsEvent) => void>;

      export const onVoicesChanged: ChromeEvent<() => void>;
    }
  }
}
