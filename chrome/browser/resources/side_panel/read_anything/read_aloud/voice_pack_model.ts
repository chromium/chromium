// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {VoiceClientSideStatusCode, VoicePackStatus} from '../voice_language_util.js';

// Holds state around languages and associated voices used by read aloud.
export class VoicePackModel {
  // Local representation of the status of voice pack downloads and
  // availability.
  private voiceStatusLocalState_: Map<string, VoiceClientSideStatusCode> =
      new Map();

  // Cache of responses from LanguagePackManager.
  private voicePackInstallStatusServerResponses_: Map<string, VoicePackStatus> =
      new Map();

  // All possible available voices for the current speech engine.
  private availableVoices_: SpeechSynthesisVoice[] = [];

  // The set of languages found in availableVoices_.
  private availableLangs_: Set<string> = new Set();

  getAvailableLangs(): Set<string> {
    return this.availableLangs_;
  }

  setAvailableLangs(langs: string[]): void {
    this.availableLangs_ = new Set(langs);
  }

  getAvailableVoices(): SpeechSynthesisVoice[] {
    return this.availableVoices_;
  }

  setAvailableVoices(voices: SpeechSynthesisVoice[]): void {
    this.availableVoices_ = voices;
  }

  getServerStatus(lang: string): VoicePackStatus|null {
    const status = this.voicePackInstallStatusServerResponses_.get(lang);
    return (status === undefined) ? null : status;
  }

  setServerStatus(lang: string, status: VoicePackStatus) {
    this.voicePackInstallStatusServerResponses_.set(lang, status);
  }

  getLocalStatus(lang: string): VoiceClientSideStatusCode|null {
    const status = this.voiceStatusLocalState_.get(lang);
    return (status === undefined) ? null : status;
  }

  setLocalStatus(lang: string, status: VoiceClientSideStatusCode) {
    this.voiceStatusLocalState_.set(lang, status);
  }

  getServerLanguages(): string[] {
    return Array.from(this.voicePackInstallStatusServerResponses_.keys());
  }
}
