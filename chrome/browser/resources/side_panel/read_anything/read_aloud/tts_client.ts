// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interfaces for TtsClients.
export interface TtsErrorEvent {
  error: string;
}

// TODO: crbug.com/498680741 - investigate refactoring
// SpeechSynthesisVoice into its own interface.
export interface TtsUtterance {
  text: string;
  voice?: SpeechSynthesisVoice;
  rate?: number;
  volume?: number;
  lang?: string;
  onEnd?: () => void;
  onBoundary?: (event: SpeechSynthesisEvent) => void;
  onError?: (event: TtsErrorEvent) => void;
  onStart?: () => void;
}

// Interface for tts clients to implement. Contains all
// necessary components to play speech from a given utterance.
export interface TtsClient {
  /**
   * Plays the given utterance.
   * @param utterance The utterance to play.
   */
  play(utterance: TtsUtterance): void;

  /**
   * Stops the current utterance.
   */
  stop(): void;

  /**
   * Pauses the current utterance.
   */
  pause(): void;

  /**
   * Resumes the current utterance.
   */
  resume(): void;

  /**
   * Returns the voices available for speech synthesis.
   */
  getVoices(): SpeechSynthesisVoice[];

  /**
   * Sets a callback to be invoked when the set of available voices changes.
   */
  setOnVoicesChanged(callback: () => void): void;

  /**
   * Returns whether the given voice is a "Natural" voice.
   */
  isNatural(voice: SpeechSynthesisVoice): boolean;

  /**
   * Returns whether the given voice is a "Google" voice.
   */
  isGoogle(voice: SpeechSynthesisVoice): boolean;

  /**
   * The priority of this client. Higher values indicate higher priority.
   */
  readonly priority: number;
}
