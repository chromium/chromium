// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {convertLangOrLocaleForVoicePackManager, getVoicePackConvertedLangIfExists} from '../voice_language_util.js';
import type {VoiceClientSideStatusCode, VoicePackStatus} from '../voice_language_util.js';
import {VoiceNotificationManager} from '../voice_notification_manager.js';

import {VoicePackModel} from './voice_pack_model.js';

export class VoicePackController {
  private notificationManager_: VoiceNotificationManager =
      VoiceNotificationManager.getInstance();
  private model_: VoicePackModel = new VoicePackModel();

  getServerStatus(lang: string): VoicePackStatus|null {
    return this.model_.getServerStatus(getVoicePackConvertedLangIfExists(lang));
  }

  setServerStatus(lang: string, status: VoicePackStatus) {
    // Convert the language string to ensure consistency across
    // languages and locales when setting the status.
    const voicePackLanguage = getVoicePackConvertedLangIfExists(lang);
    this.model_.setServerStatus(voicePackLanguage, status);
  }

  getLocalStatus(lang: string): VoiceClientSideStatusCode|null {
    return this.model_.getLocalStatus(getVoicePackConvertedLangIfExists(lang));
  }

  setLocalStatus(
      lang: string, status: VoiceClientSideStatusCode,
      availableVoices: SpeechSynthesisVoice[]) {
    const possibleVoicePackLanguage =
        convertLangOrLocaleForVoicePackManager(lang);
    const voicePackLanguage = possibleVoicePackLanguage || lang;
    const oldStatus = this.model_.getLocalStatus(voicePackLanguage);
    this.model_.setLocalStatus(voicePackLanguage, status);

    // No need for notifications for non-Google TTS languages.
    if (possibleVoicePackLanguage && (oldStatus !== status)) {
      this.notificationManager_.onVoiceStatusChange(
          voicePackLanguage, status, availableVoices);
    }
  }

  getServerLanguages(): string[] {
    return this.model_.getServerLanguages();
  }

  static getInstance(): VoicePackController {
    return instance || (instance = new VoicePackController());
  }

  static setInstance(obj: VoicePackController) {
    instance = obj;
  }
}

let instance: VoicePackController|null = null;
