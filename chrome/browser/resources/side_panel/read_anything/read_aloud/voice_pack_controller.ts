// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SpeechBrowserProxy} from '../speech_browser_proxy.js';
import {SpeechBrowserProxyImpl} from '../speech_browser_proxy.js';
import type {VoiceClientSideStatusCode, VoicePackStatus} from '../voice_language_util.js';
import {areVoicesEqual, AVAILABLE_GOOGLE_TTS_LOCALES, convertLangOrLocaleForVoicePackManager, getFilteredVoiceList, getVoicePackConvertedLangIfExists} from '../voice_language_util.js';
import {VoiceNotificationManager} from '../voice_notification_manager.js';

import {VoicePackModel} from './voice_pack_model.js';

export class VoicePackController {
  private notificationManager_: VoiceNotificationManager =
      VoiceNotificationManager.getInstance();
  private model_: VoicePackModel = new VoicePackModel();
  private speech_: SpeechBrowserProxy = SpeechBrowserProxyImpl.getInstance();

  getAvailableLangs(): string[] {
    return [...this.model_.getAvailableLangs()];
  }

  getAvailableVoices(): SpeechSynthesisVoice[] {
    return this.model_.getAvailableVoices();
  }

  hasAvailableVoices(): boolean {
    return this.getAvailableVoices().length > 0;
  }

  isVoiceAvailable(voice?: SpeechSynthesisVoice): boolean {
    return this.getAvailableVoices().some(
        availableVoice => areVoicesEqual(availableVoice, voice));
  }

  refreshAvailableVoices(forceRefresh: boolean = false): boolean {
    if (!this.hasAvailableVoices() || forceRefresh) {
      const availableVoices = getFilteredVoiceList(this.speech_.getVoices());
      this.model_.setAvailableVoices(availableVoices);
      this.model_.setAvailableLangs(availableVoices.map(({lang}) => lang));
      return true;
    }
    return false;
  }

  getDisplayNamesForLocaleCodes(): {[locale: string]: string} {
    const localeToDisplayName: {[locale: string]: string} = {};
    const langsToCheck =
        [...AVAILABLE_GOOGLE_TTS_LOCALES].concat(this.getAvailableLangs());
    for (const lang of langsToCheck) {
      const langLower = lang.toLowerCase();
      if (langLower in localeToDisplayName) {
        continue;
      }
      const langDisplayName =
          chrome.readingMode.getDisplayNameForLocale(langLower, langLower);
      if (langDisplayName) {
        localeToDisplayName[langLower] = langDisplayName;
      }
    }

    return localeToDisplayName;
  }

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

  setLocalStatus(lang: string, status: VoiceClientSideStatusCode) {
    const possibleVoicePackLanguage =
        convertLangOrLocaleForVoicePackManager(lang);
    const voicePackLanguage = possibleVoicePackLanguage || lang;
    const oldStatus = this.model_.getLocalStatus(voicePackLanguage);
    this.model_.setLocalStatus(voicePackLanguage, status);

    // No need for notifications for non-Google TTS languages.
    if (possibleVoicePackLanguage && (oldStatus !== status)) {
      this.notificationManager_.onVoiceStatusChange(
          voicePackLanguage, status, this.getAvailableVoices());
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
