// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for mojo bindings file:
 * c/r/r/e/enhanced_network_tts/enhanced_network_tts_custom_bindings.js
 * @externs
 */

declare namespace ash {
  export namespace enhancedNetworkTts {
    export namespace mojom {
      /* eslint-disable @typescript-eslint/naming-convention */
      export enum TtsRequestError {
        kEmptyUtterance = 0,
        kOverLength = 1,
        kServerError = 2,
        kReceivedUnexpectedData = 3,
        kRequestOverride = 4,
      }

      export interface TimingInfo {
        text: string;
        textOffset: number;
        timeOffset: string;
        duration: string;
      }

      export interface TtsData {
        audio: number[];
        timeInfo: TimingInfo[];
        lastData: boolean;
      }

      export interface TtsRequest {
        utterance: string;
        rate: number;
        voice: string|undefined;
        lang: string|undefined;
      }

      export interface TtsResponse {
        errorCode: TtsRequestError|undefined;
        data: TtsData|undefined;
      }
    }
  }
}

declare namespace chrome {
  export namespace mojoPrivate {
    export function requireAsync(moduleName: string): any;
  }
}

declare interface EnhancedNetworkTtsAdapter {
  getAudioDataWithCallback:
      (request: ash.enhancedNetworkTts.mojom.TtsRequest,
       callback: (response: ash.enhancedNetworkTts.mojom.TtsResponse) =>
           void) => Promise<void>;
  resetApi: () => void;
}
