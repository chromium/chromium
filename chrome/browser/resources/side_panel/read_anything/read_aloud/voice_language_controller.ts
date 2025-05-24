// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// <if expr="is_chromeos">
import {isGoogle} from '../voice_language_util.js';
// </if>
// clang-format on

import type {SpeechBrowserProxy} from '../speech_browser_proxy.js';
import {SpeechBrowserProxyImpl} from '../speech_browser_proxy.js';
import type {VoicePackStatus} from '../voice_language_util.js';
import {areVoicesEqual, AVAILABLE_GOOGLE_TTS_LOCALES, convertLangOrLocaleForVoicePackManager, convertLangOrLocaleToExactVoicePackLocale, convertLangToAnAvailableLangIfPresent, createInitialListOfEnabledLanguages, doesLanguageHaveNaturalVoices, EXTENSION_RESPONSE_TIMEOUT_MS, getFilteredVoiceList, getNaturalVoiceOrDefault, getVoicePackConvertedLangIfExists, isNatural, isVoicePackStatusError, isVoicePackStatusSuccess, mojoVoicePackStatusToVoicePackStatusEnum, VoiceClientSideStatusCode, VoicePackServerStatusErrorCode, VoicePackServerStatusSuccessCode} from '../voice_language_util.js';
import {VoiceNotificationManager} from '../voice_notification_manager.js';

import {VoiceLanguageModel} from './voice_language_model.js';

export interface VoiceLanguageListener {
  onEnabledLangsChange(): void;
  onAvailableVoicesChange(): void;
  onCurrentVoiceChange(): void;
}

export class VoiceLanguageController {
  private notificationManager_: VoiceNotificationManager =
      VoiceNotificationManager.getInstance();
  private model_: VoiceLanguageModel = new VoiceLanguageModel();
  private speech_: SpeechBrowserProxy = SpeechBrowserProxyImpl.getInstance();
  private listeners_: VoiceLanguageListener[] = [];

  // The extension is responsible for installing the Natural voices. If the
  // extension is not being responsive, the extension is probably not
  // downloaded. This handle is a reference to the callback that will be invoked
  // if the extension does not respond in a timely manner.
  private speechExtensionResponseCallbackHandle_?: number;

  constructor() {
    this.model_.setCurrentLanguage(chrome.readingMode.baseLanguageForSpeech);
    if (chrome.readingMode.isReadAloudEnabled) {
      this.speech_.setOnVoicesChanged(this.onVoicesChanged.bind(this));
    }
  }

  addListener(listener: VoiceLanguageListener) {
    this.listeners_.push(listener);
  }

  getCurrentLanguage(): string {
    return this.model_.getCurrentLanguage();
  }

  getCurrentVoice(): SpeechSynthesisVoice|null {
    return this.model_.getCurrentVoice();
  }

  private setCurrentVoice_(voice: SpeechSynthesisVoice|null): void {
    if (!areVoicesEqual(voice, this.getCurrentVoice())) {
      this.model_.setCurrentVoice(voice);
      this.listeners_.forEach(l => l.onCurrentVoiceChange());
    }
  }

  getEnabledLangs(): string[] {
    return [...this.model_.getEnabledLangs()];
  }

  getAvailableLangs(): string[] {
    return [...this.model_.getAvailableLangs()];
  }

  private setAvailableVoices_(voices: SpeechSynthesisVoice[]): void {
    this.model_.setAvailableVoices(voices);
    this.listeners_.forEach(l => l.onAvailableVoicesChange());
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

  onTtsEngineInstalled() {
    this.model_.setWaitingForNewEngine(true);
  }

  onVoicesChanged() {
    if (this.model_.getWaitingForNewEngine()) {
      this.installEnabledLangs_(
          /* onlyInstallExactGoogleLocaleMatch=*/ true,
          /* retryIfPreviousInstallFailed= */ true);
      this.model_.setWaitingForNewEngine(false);
      return;
    }

    const hadAvailableVoices = this.hasAvailableVoices();
    // Get a new list of voices. This should be done before we call
    // updateUnavailableVoiceToDefaultVoice_();
    this.refreshAvailableVoices_(/*forceRefresh=*/ true);

    // TODO: crbug.com/390435037 - Simplify logic around loading voices and
    // language availability, especially around the new TTS engine.

    // <if expr="not is_chromeos">
    this.enableNowAvailableLangs_();
    // </if>

    if (!hadAvailableVoices && this.hasAvailableVoices()) {
      // If we go from having no available voices to having voices available,
      // restore voice settings from preferences.
      this.restoreFromPrefs();
    }

    // If voice was selected automatically and not by the user, check if
    // there's a higher quality voice available now.
    this.updateAutoSelectedVoiceToNaturalVoice_();

    // If the selected voice is now unavailable, such as after an uninstall,
    // reselect a new voice.
    this.updateUnavailableVoiceToDefaultVoice_();
  }

  // Kicks off a workflow to install a language.
  // 1) Checks if the tts engine supports a version of this voice/locale
  // 2) If so, adds language for downloading
  // 3) Kicks off request GetVoicePackInfo to see if the voice is installed
  // 4) Upon response, if we see the voice is not installed and that it's in
  // the languages for downloading, then we trigger an install request
  private installLanguageIfPossible_(
      langOrLocale: string, onlyInstallExactGoogleLocaleMatch: boolean,
      retryIfPreviousInstallFailed: boolean) {
    // Don't attempt to install a language if it's not a Google TTS language
    // available for downloading. It's possible for other non-Google TTS
    // voices to have a valid language code from
    // convertLangOrLocaleForVoicePackManager, so return early instead to
    // prevent accidentally downloading untoggled voices.
    // If we shouldn't check for Google locales (such as when installing a new
    // page language), this check can be skipped.
    if (onlyInstallExactGoogleLocaleMatch &&
        !AVAILABLE_GOOGLE_TTS_LOCALES.has(langOrLocale)) {
      this.autoSwitchVoice_(langOrLocale);
      return;
    }

    const langCodeForVoicePackManager = convertLangOrLocaleForVoicePackManager(
        langOrLocale, this.getEnabledLangs(), this.getAvailableLangs());
    if (!langCodeForVoicePackManager) {
      this.autoSwitchVoice_(langOrLocale);
      return;
    }

    if (!this.requestInstall_(
            langCodeForVoicePackManager, retryIfPreviousInstallFailed)) {
      this.autoSwitchVoice_(langCodeForVoicePackManager);
    }
  }

  private autoSwitchVoice_(lang: string) {
    // Only enable this language if it has available voices and is the current
    // language. Otherwise switch to a default voice if nothing is selected.
    const availableLang =
        convertLangToAnAvailableLangIfPresent(lang, this.getAvailableLangs());
    const speechSynthesisBaseLang = this.getCurrentLanguage().split('-')[0];
    if (!availableLang ||
        (speechSynthesisBaseLang &&
         !availableLang.startsWith(speechSynthesisBaseLang))) {
      this.setUserPreferredVoiceFromPrefs_();
      return;
    }

    // Enable the preferred locale for this lang if one exists. Otherwise,
    // enable a Google TTS supported locale for this language if one exists.
    this.refreshAvailableVoices_();
    const preferredVoice = chrome.readingMode.getStoredVoice();
    const preferredVoiceLang = this.getAvailableVoices()
                                   .find(voice => voice.name === preferredVoice)
                                   ?.lang;
    let localeToEnable: string|undefined = preferredVoiceLang ?
        preferredVoiceLang :
        convertLangOrLocaleToExactVoicePackLocale(availableLang);

    // If there are no Google TTS locales for this language then enable the
    // first available locale for this language.
    if (!localeToEnable) {
      localeToEnable =
          this.getAvailableLangs().find(l => l.startsWith(availableLang));
    }

    // Enable the locales so we can select a voice for the given language and
    // show it in the voice menu.
    this.enableLang(localeToEnable);
    this.setUserPreferredVoiceFromPrefs_();
  }

  setUserPreferredVoice(selectedVoice: SpeechSynthesisVoice): void {
    this.setCurrentVoice_(selectedVoice);
    chrome.readingMode.onVoiceChange(selectedVoice.name, selectedVoice.lang);
  }

  onPageLanguageChanged() {
    const lang = chrome.readingMode.baseLanguageForSpeech;
    this.model_.setCurrentLanguage(lang);
    // Don't check for Google locales when the language has changed.
    this.installLanguageIfPossible_(
        lang,
        /* onlyInstallExactGoogleLocaleMatch=*/ false,
        /* retryIfPreviousInstallFailed= */ false);
  }

  onLanguageToggle(toggledLanguage: string) {
    const currentlyEnabled = this.isLangEnabled(toggledLanguage);

    if (!currentlyEnabled) {
      this.autoSwitchVoice_(toggledLanguage);
      this.installLanguageIfPossible_(
          toggledLanguage, /* onlyInstallExactGoogleLocaleMatch=*/ true,
          /* retryIfPreviousInstallFailed= */ true);
      this.enableLang(toggledLanguage);
    } else {
      this.uninstall_(toggledLanguage);
      this.disableLang_(toggledLanguage);
    }

    chrome.readingMode.onLanguagePrefChange(toggledLanguage, !currentlyEnabled);

    if (!currentlyEnabled) {
      // If there were no enabled languages (and thus no selected voice),
      // select a voice.
      this.getCurrentVoiceOrDefault();
    }
  }

  private setUserPreferredVoiceFromPrefs_(): void {
    const storedVoiceName = chrome.readingMode.getStoredVoice();
    if (!storedVoiceName) {
      this.setCurrentVoice_(this.getDefaultVoice_());
      return;
    }

    this.refreshAvailableVoices_();
    const selectedVoice = this.getAvailableVoices().filter(
        voice => voice.name === storedVoiceName);
    const newVoice = (selectedVoice.length && selectedVoice[0]) ?
        selectedVoice[0] :
        this.getDefaultVoice_();
    this.setCurrentVoice_(newVoice);

    // Enable the locale for the preferred voice for this language.
    this.enableLang(this.getCurrentVoice()?.lang);
  }

  private updateAutoSelectedVoiceToNaturalVoice_(): void {
    if (this.currentVoiceIsUserChosen_()) {
      return;
    }

    const naturalVoicesForLang = this.getAvailableVoices().filter(
        voice => isNatural(voice) &&
            voice.lang.startsWith(this.getCurrentLanguage()));
    if (!naturalVoicesForLang.length || !naturalVoicesForLang[0]) {
      return;
    }

    this.setCurrentVoice_(naturalVoicesForLang[0]);
  }

  // Checks the install status of the current voice and updates to the
  // default voice if it's no longer available.
  private updateUnavailableVoiceToDefaultVoice_(): void {
    for (const lang of this.model_.getServerLanguages()) {
      this.requestInfo_(lang);
    }
    const currentVoice = this.getCurrentVoice();
    if (currentVoice && !this.isVoiceAvailable(currentVoice)) {
      this.setCurrentVoice_(this.getDefaultVoice_());
    }
  }

  getCurrentVoiceOrDefault(): SpeechSynthesisVoice|null {
    const currentVoice = this.getCurrentVoice();
    if (!currentVoice) {
      this.setCurrentVoice_(this.getDefaultVoice_());
    }

    return this.getCurrentVoice();
  }

  onLanguageUnavailableError(): void {
    const possibleNewLanguage = convertLangToAnAvailableLangIfPresent(
        this.getCurrentLanguage(), this.getAvailableLangs(),
        /* allowCurrentLanguageIfExists */ false);
    if (possibleNewLanguage) {
      this.model_.setCurrentLanguage(possibleNewLanguage);
    }
  }

  // Attempt to get a new voice using the current language. In theory, the
  // previously unavailable voice should no longer be showing up in
  // availableVoices, but we ensure that the alternative voice does not match
  // the previously unavailable voice as an extra measure. This method should
  // only be called when speech synthesis returns an error.
  onVoiceUnavailableError(): void {
    const currentVoice = this.getCurrentVoice();
    const newVoice = this.getDefaultVoice_();

    // If the default voice is not the same as the original, unavailable voice,
    // use that, only if the new voice is also defined.
    if (newVoice && !areVoicesEqual(newVoice, currentVoice)) {
      this.setCurrentVoice_(newVoice);
      return;
    }

    // If the default voice won't work, try another voice in that language.
    const baseLang = this.getCurrentLanguage();
    this.refreshAvailableVoices_();
    const voicesForLanguage = this.getAvailableVoices().filter(
        voice => voice.lang.startsWith(baseLang));

    // TODO: crbug.com/40927698 - It's possible we can get stuck in an infinite
    // loop of jumping back and forth between two or more invalid voices, if
    // multiple voices are invalid. Investigate if we need to do more to handle
    // this case.

    // TODO: crbug.com/336596926 - If there still aren't voices for the
    // language, attempt to fallback to the browser language, if we're using
    // the page language.
    let voiceIndex = 0;
    while (voiceIndex < voicesForLanguage.length) {
      if (!areVoicesEqual(voicesForLanguage[voiceIndex], currentVoice)) {
        // Return another voice in the same language, ensuring we're not
        // returning the previously unavailable voice for extra safety.
        this.setCurrentVoice_(voicesForLanguage[voiceIndex] || null);
        return;
      }
      voiceIndex++;
    }

    // TODO: crbug.com/336596926 - Handle language updates if there aren't any
    // available voices in the current language other than the unavailable
    // voice.
    this.setCurrentVoice_(null);
  }

  private disableLangIfNoVoices_(lang: string): void {
    const lowerLang = lang.toLowerCase();
    this.refreshAvailableVoices_();
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
          this.disableLang_(enabledLang);
        }
      });
    }
  }

  private disableLang_(lang?: string): void {
    if (!lang) {
      return;
    }
    if (this.isLangEnabled(lang)) {
      this.model_.disableLang(lang);
      this.listeners_.forEach(l => l.onEnabledLangsChange());
    }
  }

  enableLang(lang?: string): void {
    if (!lang) {
      return;
    }
    if (!this.isLangEnabled(lang)) {
      this.model_.enableLang(lang.toLowerCase());
      this.listeners_.forEach(l => l.onEnabledLangsChange());
    }
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
  private enableNowAvailableLangs_(): void {
    const nowAvailableLangs =
        [...this.model_.getPossiblyDisabledLangs()].filter(
            (lang: string) => this.isLangAvailable_(lang));
    nowAvailableLangs.forEach(lang => {
      const lowerLang = lang.toLowerCase();
      this.enableLang(lowerLang);
      chrome.readingMode.onLanguagePrefChange(lowerLang, true);
      this.model_.removePossiblyDisabledLang(lowerLang);
    });
  }

  private isLangAvailable_(lang: string) {
    return this.model_.getAvailableLangs().has(lang.toLowerCase());
  }
  // </if>

  restoreFromPrefs(): void {
    // We need to make sure the languages we choose correspond to voices, so
    // refresh the list of voices and available langs
    this.refreshAvailableVoices_();
    this.model_.setCurrentLanguage(chrome.readingMode.baseLanguageForSpeech);
    const storedLanguagesPref = chrome.readingMode.getLanguagesEnabledInPref();
    const langOfDefaultVoice = this.getDefaultVoice_()?.lang;

    // We need to restore enabled languages prior to selecting the preferred
    // voice to ensure we have the right voices available, and prior to updating
    // the preferences so we can check against what's available and enabled.
    const langs = createInitialListOfEnabledLanguages(
        chrome.readingMode.baseLanguageForSpeech, storedLanguagesPref,
        this.getAvailableLangs(), langOfDefaultVoice);
    langs.forEach((l: string) => this.enableLang(l));

    this.installEnabledLangs_(/* onlyInstallExactGoogleLocaleMatch=*/ true,
                              /* retryIfPreviousInstallFailed= */ false);
    this.setUserPreferredVoiceFromPrefs_();
    this.alignPreferencesWithEnabledLangs_(storedLanguagesPref);
  }

  private refreshAvailableVoices_(forceRefresh: boolean = false): void {
    if (!this.hasAvailableVoices() || forceRefresh) {
      const availableVoices = getFilteredVoiceList(this.speech_.getVoices());
      this.setAvailableVoices_(availableVoices);
      this.model_.setAvailableLangs(availableVoices.map(({lang}) => lang));
    }
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

  updateLanguageStatus(lang: string, status: string) {
    this.stopWaitingForSpeechExtension();
    if (!lang.length) {
      return;
    }

    const newStatus = mojoVoicePackStatusToVoicePackStatusEnum(status);
    this.setServerStatus(lang, newStatus);
    this.updateApplicationState_(lang, newStatus);

    if (isVoicePackStatusError(newStatus)) {
      this.disableLangIfNoVoices_(lang);
    }
  }

  // Store client side language state and trigger side effects.
  private updateApplicationState_(lang: string, newStatus: VoicePackStatus) {
    if (isVoicePackStatusSuccess(newStatus)) {
      const newStatusCode = newStatus.code;

      switch (newStatusCode) {
        case VoicePackServerStatusSuccessCode.NOT_INSTALLED:
          this.triggerInstall_(lang);
          break;
        case VoicePackServerStatusSuccessCode.INSTALLING:
          // Do nothing- we mark our local state as installing when we send the
          // request. Locally, we may time out a slow request and mark it as
          // errored, and we don't want to overwrite that state here.
          break;
        case VoicePackServerStatusSuccessCode.INSTALLED:
          // Force a refresh of the voices list since we might not get an update
          // the voices have changed.
          this.refreshAvailableVoices_(/*forceRefresh=*/ true);
          this.autoSwitchVoice_(lang);

          // Some languages may require a download from the tts engine
          // but may not have associated natural voices.
          const languageHasNaturalVoices = doesLanguageHaveNaturalVoices(lang);

          // Even though the voice may be installed on disk, it still may not be
          // available to the speechSynthesis API. Check whether to mark the
          // voice as AVAILABLE or INSTALLED_AND_UNAVAILABLE
          const voicesForLanguageAreAvailable = this.getAvailableVoices().some(
              voice =>
                  ((isNatural(voice) || !languageHasNaturalVoices) &&
                   getVoicePackConvertedLangIfExists(voice.lang) === lang));

          // If natural voices are currently available for the language or the
          // language does not support natural voices, set the status to
          // available. Otherwise, set the status to install and unavailabled.
          this.setLocalStatus(
              lang,
              voicesForLanguageAreAvailable ?
                  VoiceClientSideStatusCode.AVAILABLE :
                  VoiceClientSideStatusCode.INSTALLED_AND_UNAVAILABLE);
          break;
        default:
          // This ensures the switch statement is exhaustive
          return newStatusCode satisfies never;
      }
    } else if (isVoicePackStatusError(newStatus)) {
      this.autoSwitchVoice_(lang);
      const newStatusCode = newStatus.code;

      switch (newStatusCode) {
        case VoicePackServerStatusErrorCode.OTHER:
        case VoicePackServerStatusErrorCode.WRONG_ID:
        case VoicePackServerStatusErrorCode.NEED_REBOOT:
        case VoicePackServerStatusErrorCode.UNSUPPORTED_PLATFORM:
          this.setLocalStatus(lang, VoiceClientSideStatusCode.ERROR_INSTALLING);
          break;
        case VoicePackServerStatusErrorCode.ALLOCATION:
          this.setLocalStatus(
              lang, VoiceClientSideStatusCode.INSTALL_ERROR_ALLOCATION);
          break;
        default:
          // This ensures the switch statement is exhaustive
          return newStatusCode satisfies never;
      }
    } else {
      // Couldn't parse the response
      this.setLocalStatus(lang, VoiceClientSideStatusCode.ERROR_INSTALLING);
    }
  }

  private triggerInstall_(language: string) {
    // Install the voice if it's not currently installed and it's marked
    // as a language that should be installed
    if (this.model_.hasLanguageForDownload(language)) {
      // Don't re-send install request if it's already been sent
      if (this.getLocalStatus(language) !==
          VoiceClientSideStatusCode.SENT_INSTALL_REQUEST) {
        this.forceInstallRequest_(language, /* isRetry = */ false);
      }
    } else {
      this.setLocalStatus(language, VoiceClientSideStatusCode.NOT_INSTALLED);
    }
  }

  // Returns true if this requested either an install or more info.
  private requestInstall_(
      language: string, retryIfPreviousInstallFailed: boolean): boolean {
    const serverStatus = this.getServerStatus(language);
    if (!serverStatus) {
      if (retryIfPreviousInstallFailed) {
        this.forceInstallRequest_(language, /* isRetry = */ false);
      } else {
        this.model_.addLanguageForDownload(language);
        this.requestInfo_(language);
      }
      return true;
    }

    if (isVoicePackStatusSuccess(serverStatus) &&
        serverStatus.code === VoicePackServerStatusSuccessCode.NOT_INSTALLED) {
      this.model_.addLanguageForDownload(language);
      this.requestInfo_(language);
      return true;
    }

    if (retryIfPreviousInstallFailed && isVoicePackStatusError(serverStatus)) {
      this.model_.addLanguageForDownload(language);

      // If the previous install attempt failed (e.g. due to no internet
      // connection), the PackManager sends a failure for subsequent GetInfo
      // requests. Therefore, bypass the normal flow of calling
      // GetInfo to see if the voice is available to install, and just call
      // sendInstallVoicePackRequest directly
      this.forceInstallRequest_(language, /* isRetry = */ true);
      return true;
    }

    return false;
  }

  private uninstall_(langOrLocale: string) {
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

  private installEnabledLangs_(
      onlyInstallExactGoogleLocaleMatch: boolean,
      retryIfPreviousInstallFailed: boolean) {
    for (const lang of this.getEnabledLangs()) {
      this.installLanguageIfPossible_(
          lang, onlyInstallExactGoogleLocaleMatch,
          retryIfPreviousInstallFailed);
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

  private forceInstallRequest_(language: string, isRetry: boolean) {
    this.setLocalStatus(
        language,
        isRetry ? VoiceClientSideStatusCode.SENT_INSTALL_REQUEST_ERROR_RETRY :
                  VoiceClientSideStatusCode.SENT_INSTALL_REQUEST);

    chrome.readingMode.sendInstallVoicePackRequest(language);
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

  private currentVoiceIsUserChosen_(): boolean {
    const storedVoiceName = chrome.readingMode.getStoredVoice();

    // getCurrentVoice() is not necessarily chosen by the user, it is just
    // the voice that read aloud is using. It may be a default voice chosen by
    // read aloud, so we check it against user preferences to see if it was
    // user-chosen.
    if (storedVoiceName) {
      return this.getCurrentVoice()?.name === storedVoiceName;
    }
    return false;
  }

  private getDefaultVoice_(): SpeechSynthesisVoice|null {
    this.refreshAvailableVoices_();
    const allPossibleVoices = this.getAvailableVoices();
    const voicesForLanguage = allPossibleVoices.filter(
        voice => voice.lang.startsWith(this.getCurrentLanguage()));

    if (!voicesForLanguage.length) {
      // Stay with the current voice if no voices are available for this
      // language.
      return this.getCurrentVoice() ?
          this.getCurrentVoice() :
          getNaturalVoiceOrDefault(allPossibleVoices);
    }

    // First try to choose a voice only from currently enabled locales for this
    // language.
    const voicesForCurrentEnabledLocale =
        voicesForLanguage.filter(v => this.isLangEnabled(v.lang));
    if (!voicesForCurrentEnabledLocale.length) {
      // If there's no enabled locales for this language, check for any other
      // voices for enabled locales.
      const allVoicesForEnabledLocales =
          allPossibleVoices.filter(v => this.isLangEnabled(v.lang));
      if (!allVoicesForEnabledLocales.length) {
        // If there are no voices for the enabled locales, or no enabled
        // locales at all, we can't select a voice. So return null so we
        // can disable the play button.
        return null;
      } else {
        return getNaturalVoiceOrDefault(allVoicesForEnabledLocales);
      }
    }

    return getNaturalVoiceOrDefault(voicesForCurrentEnabledLocale);
  }

  static getInstance(): VoiceLanguageController {
    return instance || (instance = new VoiceLanguageController());
  }

  static setInstance(obj: VoiceLanguageController) {
    instance = obj;
  }
}

let instance: VoiceLanguageController|null = null;
