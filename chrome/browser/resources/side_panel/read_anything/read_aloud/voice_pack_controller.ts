// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SpeechBrowserProxy} from '../speech_browser_proxy.js';
import {SpeechBrowserProxyImpl} from '../speech_browser_proxy.js';
import type {VoicePackStatus} from '../voice_language_util.js';
import {areVoicesEqual, AVAILABLE_GOOGLE_TTS_LOCALES, convertLangOrLocaleForVoicePackManager, createInitialListOfEnabledLanguages, EXTENSION_RESPONSE_TIMEOUT_MS, getFilteredVoiceList, getVoicePackConvertedLangIfExists, isVoicePackStatusError, isVoicePackStatusSuccess, VoiceClientSideStatusCode, VoicePackServerStatusSuccessCode} from '../voice_language_util.js';
import {VoiceNotificationManager} from '../voice_notification_manager.js';

import {VoicePackModel} from './voice_pack_model.js';

// clang-format off
// <if expr="is_chromeos">
import {isGoogle} from '../voice_language_util.js';
// </if>
// clang-format on


export class VoicePackController {
  private notificationManager_: VoiceNotificationManager =
      VoiceNotificationManager.getInstance();
  private model_: VoicePackModel = new VoicePackModel();
  private speech_: SpeechBrowserProxy = SpeechBrowserProxyImpl.getInstance();

  // The extension is responsible for installing the Natural voices. If the
  // extension is not being responsive, the extension is probably not
  // downloaded. This handle is a reference to the callback that will be invoked
  // if the extension does not respond in a timely manner.
  private speechExtensionResponseCallbackHandle_?: number;

  getEnabledLangs(): string[] {
    return [...this.model_.getEnabledLangs()];
  }

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

  disableLangIfNoVoices(lang: string): boolean {
    const lowerLang = lang.toLowerCase();
    this.refreshAvailableVoices();
    const availableVoicesForLang = this.getAvailableVoicesForLang_(lowerLang);

    let disableLang = false;
    // <if expr="is_chromeos">
    disableLang = !availableVoicesForLang.some(voice => isGoogle(voice));
    // </if>
    // <if expr="not is_chromeos">
    disableLang = availableVoicesForLang.length === 0;
    // </if>

    if (disableLang) {
      chrome.readingMode.onLanguagePrefChange(lowerLang, false);
      this.getEnabledLangs().forEach(enabledLang => {
        if (getVoicePackConvertedLangIfExists(enabledLang) === lowerLang) {
          this.disableLang(enabledLang);
        }
      });
    }

    return disableLang;
  }

  // Returns whether lang was enabled before disabling it.
  disableLang(lang?: string): boolean {
    if (!lang) {
      return false;
    }
    return this.model_.disableLang(lang);
  }

  // Returns whether lang was disabled before enabling it.
  enableLang(lang?: string): boolean {
    if (!lang) {
      return false;
    }
    if (!this.isLangEnabled(lang)) {
      this.model_.enableLang(lang.toLowerCase());
      return true;
    }
    return false;
  }

  isLangEnabled(lang: string): boolean {
    return this.model_.getEnabledLangs().has(lang.toLowerCase());
  }

  // If we disabled a language during startup because it wasn't yet available,
  // we should re-enable it once it's available. This can happen if we enable
  // a language with natural voices but no system voices. This only needs to
  // happen on non-ChromeOS, since we're only installing the new engine
  // outside of ChromeOS.
  // <if expr="not is_chromeos">
  enableNowAvailableLangs(): boolean {
    const nowAvailableLangs =
        [...this.model_.getPossiblyDisabledLangs()].filter(
            (lang: string) => this.isLangAvailable_(lang));
    nowAvailableLangs.forEach(lang => {
      const lowerLang = lang.toLowerCase();
      this.enableLang(lowerLang);
      chrome.readingMode.onLanguagePrefChange(lowerLang, true);
      this.model_.removePossiblyDisabledLang(lowerLang);
    });
    return nowAvailableLangs.length > 0;
  }

  private isLangAvailable_(lang: string) {
    return this.model_.getAvailableLangs().has(lang.toLowerCase());
  }
  // </if>

  getInitialListOfEnabledLanguages(langOfDefaultVoice?: string): string[] {
    const storedLanguagesPref = chrome.readingMode.getLanguagesEnabledInPref();
    const langs = createInitialListOfEnabledLanguages(
        chrome.readingMode.baseLanguageForSpeech, storedLanguagesPref,
        this.getAvailableLangs(), langOfDefaultVoice);
    this.alignPreferencesWithEnabledLangs_(storedLanguagesPref);
    langs.forEach((l: string) => this.enableLang(l));
    return langs;
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

  refreshVoicePackStatuses() {
    for (const lang of this.model_.getServerLanguages()) {
      this.requestInfo_(lang);
    }
  }

  triggerInstall(voicePackLanguage: string) {
    // Install the voice if it's not currently installed and it's marked
    // as a language that should be installed
    if (this.model_.hasLanguageForDownload(voicePackLanguage)) {
      // Don't re-send install request if it's already been sent
      if (this.getLocalStatus(voicePackLanguage) !==
          VoiceClientSideStatusCode.SENT_INSTALL_REQUEST) {
        this.forceInstallRequest_(voicePackLanguage, /* isRetry = */ false);
      }
    } else {
      this.setLocalStatus(
          voicePackLanguage, VoiceClientSideStatusCode.NOT_INSTALLED);
    }
  }

  // Returns true if this requested either an install or more info.
  requestInstall(
      voicePackLanguage: string,
      retryIfPreviousInstallFailed: boolean): boolean {
    const serverStatus = this.getServerStatus(voicePackLanguage);
    if (!serverStatus) {
      if (retryIfPreviousInstallFailed) {
        this.forceInstallRequest_(voicePackLanguage, /* isRetry = */ false);
      } else {
        // Inquire if the voice pack is downloaded. If not, it'll trigger a
        // download in updateVoicePackStatus().
        this.model_.addLanguageForDownload(voicePackLanguage);
        this.requestInfo_(voicePackLanguage);
      }
      return true;
    }

    if (isVoicePackStatusSuccess(serverStatus) &&
        serverStatus.code === VoicePackServerStatusSuccessCode.NOT_INSTALLED) {
      // Inquire if the voice pack is downloaded. If not, it'll trigger a
      // download in updateVoicePackStatus().
      this.model_.addLanguageForDownload(voicePackLanguage);
      this.requestInfo_(voicePackLanguage);
      return true;
    }

    if (retryIfPreviousInstallFailed && isVoicePackStatusError(serverStatus)) {
      this.model_.addLanguageForDownload(voicePackLanguage);

      // If the previous install attempt failed (e.g. due to no internet
      // connection), the PackManager sends a failure for subsequent GetInfo
      // requests. Therefore, bypass the normal flow of calling
      // GetInfo to see if the voice is available to install, and just call
      // sendInstallVoicePackRequest directly
      this.forceInstallRequest_(voicePackLanguage, /* isRetry = */ true);
      return true;
    }

    return false;
  }

  uninstall(langOrLocale: string) {
    const voicePackLang = convertLangOrLocaleForVoicePackManager(langOrLocale);
    if (voicePackLang) {
      this.notificationManager_.onCancelDownload(voicePackLang);
      this.model_.removeLanguageForDownload(voicePackLang);
      chrome.readingMode.sendUninstallVoiceRequest(voicePackLang);
    }
  }

  stopWaitingForSpeechExtension() {
    if (this.speechExtensionResponseCallbackHandle_ !== undefined) {
      clearTimeout(this.speechExtensionResponseCallbackHandle_);
      this.speechExtensionResponseCallbackHandle_ = undefined;
    }
  }

  private requestInfo_(langOrLocale: string) {
    const langOrLocaleForPackManager =
        convertLangOrLocaleForVoicePackManager(langOrLocale);
    if (langOrLocaleForPackManager) {
      this.setSpeechExtensionResponseTimeout_();
      chrome.readingMode.sendGetVoicePackInfoRequest(
          langOrLocaleForPackManager);
    }
  }

  private forceInstallRequest_(voicePackLanguage: string, isRetry: boolean) {
    this.setLocalStatus(
        voicePackLanguage,
        isRetry ? VoiceClientSideStatusCode.SENT_INSTALL_REQUEST_ERROR_RETRY :
                  VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);

    chrome.readingMode.sendInstallVoicePackRequest(voicePackLanguage);
  }

  // Schedules a timer that will notify the user if the speech extension is
  // unresponsive. Only schedules a new timer if there is none pending.
  private setSpeechExtensionResponseTimeout_() {
    if (this.speechExtensionResponseCallbackHandle_ === undefined) {
      this.speechExtensionResponseCallbackHandle_ = setTimeout(
          () => this.notificationManager_.onNoEngineConnection(),
          EXTENSION_RESPONSE_TIMEOUT_MS);
    }
  }

  private alignPreferencesWithEnabledLangs_(languagesInPref: string[]) {
    // Only update the unavailable languages in prefs if there are any
    // available languages. Otherwise, we should wait until the available
    // languages are updated to do this.
    if (!this.model_.getAvailableLangs().size) {
      return;
    }

    // If a stored language doesn't have a match in the enabled languages
    // list, disable the original preference. If a particular locale becomes
    // unavailable between reading mode sessions, we may enable a different
    // locale instead, and the now unavailable locale can never be removed
    // by the user, so remove it here and save the newly enabled locale. For
    // example if the user previously enabled 'pt-pt' and now it is
    // unavailable, createInitialListOfEnabledLanguages above will enable
    // 'pt-br' instead if it is available. Thus we should remove 'pt-pt' from
    // preferences here and add 'pt-br' below.
    languagesInPref.forEach(storedLanguage => {
      if (!this.isLangEnabled(storedLanguage)) {
        chrome.readingMode.onLanguagePrefChange(storedLanguage, false);

        // Keep track of these languages in case they become available
        // after the TTS engine extension is installed.
        // <if expr="not is_chromeos">
        this.model_.addPossiblyDisabledLang(storedLanguage.toLowerCase());
        // </if>
      }
    });
    this.model_.getEnabledLangs().forEach(
        enabledLanguage =>
            chrome.readingMode.onLanguagePrefChange(enabledLanguage, true));
  }

  private getAvailableVoicesForLang_(lang: string): SpeechSynthesisVoice[] {
    return this.model_.getAvailableVoices().filter(
        v => getVoicePackConvertedLangIfExists(v.lang) === lang);
  }

  static getInstance(): VoicePackController {
    return instance || (instance = new VoicePackController());
  }

  static setInstance(obj: VoicePackController) {
    instance = obj;
  }
}

let instance: VoicePackController|null = null;
