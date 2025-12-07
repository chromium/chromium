// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {VoiceClientSideStatusCode, VoicePackStatus} from './voice_language_conversions.js';

// Holds state around languages and associated voices used by read aloud.
export class VoiceLanguageModel {
  // Local representation of the status of language downloads and availability.
  private localInstallState_: Map<string, VoiceClientSideStatusCode> =
      new Map();

  // Cache of responses from the tts engine.
  private cachedServerInstallState_: Map<string, VoicePackStatus> = new Map();

  // All possible available voices for the current speech engine.
  private availableVoices_: SpeechSynthesisVoice[] = [];

  // The set of languages found in availableVoices_.
  private availableLangs_: Set<string> = new Set();

  // The set of languages currently enabled for use by Read Aloud. This
  // includes user-enabled languages and auto-downloaded languages. The former
  // are stored in preferences. The latter are not.
  private enabledLangs_: Set<string> = new Set();

  // These are languages that don't exist when restoreEnabledLanguagesFromPref()
  // is first called when the engine is getting set up. We need to disable
  // unavailable languages, but since it's possible that these languages may
  // become available once the TTS engine finishes setting up, we want to save
  // them so they can be used as soon as they are available. This can happen
  // when a natural voice is installed (e.g. Danish) when there isn't an
  // equivalent system voice.
  // <if expr="not is_chromeos">
  private possiblyDisabledLangs_: Set<string> = new Set();
  // </if>

  // Set of languages of the browser and/or of the pages navigated to that we
  // need to download Natural voices for automatically
  private languagesForVoiceDownloads_: Set<string> = new Set();

  // When a new TTS Engine extension is loaded into reading mode, we want to try
  // to install new natural voices from it. However, the new engine may not be
  // ready until it calls onvoiceschanged, so set this and
  // request the install when that's called.
  private waitingForNewEngine_ = false;

  private currentVoice_: SpeechSynthesisVoice|null = null;
  private currentLanguage_: string = '';

  getWaitingForNewEngine(): boolean {
    return this.waitingForNewEngine_;
  }

  setWaitingForNewEngine(waitingForNewEngine: boolean): void {
    this.waitingForNewEngine_ = waitingForNewEngine;
  }

  addLanguageForDownload(lang: string): void {
    this.languagesForVoiceDownloads_.add(lang);
  }

  removeLanguageForDownload(lang: string): void {
    this.languagesForVoiceDownloads_.delete(lang);
  }

  hasLanguageForDownload(lang: string): boolean {
    return this.languagesForVoiceDownloads_.has(lang);
  }

  getEnabledLangs(): Set<string> {
    return this.enabledLangs_;
  }

  enableLang(lang: string): void {
    this.enabledLangs_.add(lang);
  }

  disableLang(lang: string): void {
    this.enabledLangs_.delete(lang);
  }

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

  // <if expr="not is_chromeos">
  getPossiblyDisabledLangs(): Set<string> {
    return this.possiblyDisabledLangs_;
  }

  addPossiblyDisabledLang(lang: string): void {
    this.possiblyDisabledLangs_.add(lang);
  }

  removePossiblyDisabledLang(lang: string): void {
    this.possiblyDisabledLangs_.delete(lang);
  }
  // </if>

  getServerStatus(lang: string): VoicePackStatus|null {
    const status = this.cachedServerInstallState_.get(lang);
    return (status === undefined) ? null : status;
  }

  setServerStatus(lang: string, status: VoicePackStatus) {
    this.cachedServerInstallState_.set(lang, status);
  }

  getLocalStatus(lang: string): VoiceClientSideStatusCode|null {
    const status = this.localInstallState_.get(lang);
    return (status === undefined) ? null : status;
  }

  setLocalStatus(lang: string, status: VoiceClientSideStatusCode) {
    this.localInstallState_.set(lang, status);
  }

  getServerLanguages(): string[] {
    return Array.from(this.cachedServerInstallState_.keys());
  }

  getCurrentVoice(): SpeechSynthesisVoice|null {
    return this.currentVoice_ || null;
  }

  setCurrentVoice(voice: SpeechSynthesisVoice|null): void {
    this.currentVoice_ = voice;
  }

  getCurrentLanguage(): string {
    return this.currentLanguage_;
  }

  setCurrentLanguage(language: string): void {
    this.currentLanguage_ = language;
  }
}
