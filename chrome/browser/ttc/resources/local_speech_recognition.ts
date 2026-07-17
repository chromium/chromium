// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {log, warnLog} from './logging.js';

const FILE = 'LocalSpeechRecognition';

// Ambient declarations for Web Speech API in Chromium (#9)
declare interface SpeechRecognitionEvent extends Event {
  readonly resultIndex: number;
  readonly results: SpeechRecognitionResultList;
}
declare interface SpeechRecognitionErrorEvent extends Event {
  readonly error: string;
}
declare interface SpeechRecognition extends EventTarget {
  continuous: boolean;
  interimResults: boolean;
  lang: string;
  onstart: (() => void)|null;
  onaudiostart: (() => void)|null;
  onsoundstart: (() => void)|null;
  onspeechstart: (() => void)|null;
  onresult: ((event: SpeechRecognitionEvent) => void)|null;
  onerror: ((event: SpeechRecognitionErrorEvent) => void)|null;
  onend: (() => void)|null;
  start(): void;
  stop(): void;
  abort(): void;
}

export interface LocalSpeechRecognitionDelegate {
  onLocalTranscript: (text: string) => void;
  isAssistantSpeaking: () => boolean;
  isSessionListening: () => boolean;
}

/**
 * Encapsulates browser-local Web Speech API (SODA/cloud ASR) recognition.
 */
export class LocalSpeechRecognition {
  private recognition: SpeechRecognition|null = null;
  private delegate: LocalSpeechRecognitionDelegate;

  constructor(delegate: LocalSpeechRecognitionDelegate) {
    this.delegate = delegate;
    this.init();
  }

  private init() {
    // Since this WebUI strictly runs inside Chromium, we check known
    // constructors (#10).
    const SpeechRecognitionClass =
        (window as unknown as {
          SpeechRecognition?: {new (): SpeechRecognition}
        }).SpeechRecognition ||
        (window as unknown as {
          webkitSpeechRecognition?: {new (): SpeechRecognition}
        }).webkitSpeechRecognition;
    if (!SpeechRecognitionClass) {
      log(FILE, 'Web Speech API constructor not found.');
      return;
    }

    this.recognition = new SpeechRecognitionClass();
    this.recognition.continuous = true;
    this.recognition.interimResults = true;
    this.recognition.lang = 'en-US';

    log(FILE, 'Speech recognition initialized successfully.');

    this.recognition.onstart = () => {
      log(FILE, 'Speech recognition service started (listening).');
    };

    this.recognition.onaudiostart = () => {
      log(FILE, 'Audio capture started (mic is recording).');
    };

    this.recognition.onsoundstart = () => {
      log(FILE, 'Sound detected (audio levels rising).');
    };

    this.recognition.onspeechstart = () => {
      log(FILE, 'Speech detected.');
    };

    this.recognition.onresult = (event: SpeechRecognitionEvent) => {
      let interimTranscript = '';
      let finalTranscript = '';

      for (let i = event.resultIndex; i < event.results.length; ++i) {
        const result = event.results[i];
        if (result && result.isFinal) {
          finalTranscript += result[0]?.transcript || '';
        } else if (result) {
          interimTranscript += result[0]?.transcript || '';
        }
      }

      const activeTranscript = finalTranscript + interimTranscript;
      log(FILE,
          `Speech recognition result: "${activeTranscript}" (isFinal: ${
              event.results[event.resultIndex]?.isFinal})`);

      if (activeTranscript.trim()) {
        if (this.delegate.isAssistantSpeaking()) {
          // Ignore local SODA results while the assistant is speaking to
          // prevent acoustic feedback or echo transcripts (#12).
          return;
        }
        this.delegate.onLocalTranscript(activeTranscript);
      }
    };

    this.recognition.onerror = (event: SpeechRecognitionErrorEvent) => {
      log(FILE, 'Speech recognition error:', event.error);
      if (event.error === 'not-allowed' ||
          event.error === 'service-not-allowed') {
        warnLog(
            FILE,
            'Speech recognition permission denied or service unavailable. Disabling local STT and falling back to cloud ASR.');
        this.recognition = null;
      }
    };

    this.recognition.onend = () => {
      log(FILE, 'Speech recognition ended.');
      if (this.recognition && this.delegate.isSessionListening()) {
        try {
          this.recognition.start();
        } catch (e) {
          // Already started
        }
      }
    };
  }

  start() {
    if (this.recognition) {
      try {
        this.recognition.start();
      } catch (e) {
        // Already started
      }
    }
  }

  stop() {
    if (this.recognition) {
      try {
        this.recognition.stop();
      } catch (e) {
        // Already stopped
      }
    }
  }
}
