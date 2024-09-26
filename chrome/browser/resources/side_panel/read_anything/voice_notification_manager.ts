// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getNotification, NotificationType} from './voice_language_util.js';
import type {VoiceClientSideStatusCode} from './voice_language_util.js';

export interface VoiceNotificationListener {
  // Listeners should notify via their UI of language pack status changes.
  notify(language: string, type: NotificationType): void;
}

// Notifies listeners of language pack status changes.
export class VoiceNotificationManager {
  private listeners_: Set<VoiceNotificationListener> = new Set();

  // Separately keep track of languages in the middle of downloading as we want
  // to always show that notification.
  private downloadingLanguages_: Set<string> = new Set();

  addListener(listener: VoiceNotificationListener) {
    this.listeners_.add(listener);
    // Tell listeners of all current downloading languages.
    this.downloadingLanguages_.forEach(
        language => listener.notify(language, NotificationType.DOWNLOADING));
  }

  removeListener(listener: VoiceNotificationListener) {
    this.listeners_.delete(listener);
  }

  clear() {
    this.listeners_.clear();
    this.downloadingLanguages_.clear();
  }

  onVoiceStatusChange(
      language: string, status: VoiceClientSideStatusCode,
      availableVoices: SpeechSynthesisVoice[],
      onLine: boolean = window.navigator.onLine) {
    const notification =
        getNotification(language, status, availableVoices, onLine);
    if (notification === NotificationType.DOWNLOADING) {
      this.downloadingLanguages_.add(language);
    } else if (this.downloadingLanguages_.has(language)) {
      this.downloadingLanguages_.delete(language);
    }

    this.listeners_.forEach(
        listener => listener.notify(language, notification));
  }

  static getInstance(): VoiceNotificationManager {
    return instance || (instance = new VoiceNotificationManager());
  }

  static setInstance(obj: VoiceNotificationManager) {
    instance = obj;
  }
}

let instance: VoiceNotificationManager|null = null;
