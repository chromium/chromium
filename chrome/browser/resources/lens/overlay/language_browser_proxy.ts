// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';

import type {BrowserProxy} from './browser_proxy.js';
import type {Language} from './translate.mojom-webui.js';

/**
 * @fileoverview A browser proxy for fetching language settings.
 */
let instance: LanguageBrowserProxy|null = null;

const LAST_FETCH_SUPPORTED_LANGUAGES_TIME_KEY =
    'lastFetchSupportedLanguagesTime';
const SUPPORTED_LANGUAGES_LOCALE_KEY = 'supportedLanguagesLocale';
const SUPPORTED_SOURCE_LANGUAGES_KEY = 'supportedSourceLanguages';
const SUPPORTED_TARGET_LANGUAGES_KEY = 'supportedTargetLanguages';

export interface LanguageBrowserProxy {
  getClientLanguageList(): Promise<Language[]>;
  getTranslateTargetLanguage(): Promise<string>;
  getStoredServerLanguages(browserProxy: BrowserProxy):
      Promise<{sourceLanguages: Language[], targetLanguages: Language[]}>;
  storeLanguages(
      locale: string, sourceLanguages: Language[],
      targetLanguages: Language[]): void;
}

export class LanguageBrowserProxyImpl implements LanguageBrowserProxy {
  static getInstance(): LanguageBrowserProxy {
    return instance || (instance = new LanguageBrowserProxyImpl());
  }

  static setInstance(obj: LanguageBrowserProxy) {
    instance = obj;
  }

  getClientLanguageList(): Promise<Language[]> {
    return chrome.languageSettingsPrivate.getLanguageList().then(
        (clientLanguageList: chrome.languageSettingsPrivate.Language[]) => {
          return clientLanguageList.map(lang => ({
                                          languageCode: lang.code,
                                          name: lang.displayName,
                                        }));
        });
  }

  getTranslateTargetLanguage(): Promise<string> {
    return chrome.languageSettingsPrivate.getTranslateTargetLanguage();
  }

  getStoredServerLanguages(browserProxy: BrowserProxy):
      Promise<{sourceLanguages: Language[], targetLanguages: Language[]}> {
    if (this.shouldFetchSupportedLanguages()) {
      return browserProxy.handler.fetchSupportedLanguages().then(
          this.onServerLanguageListFetched.bind(this));
    }

    const sourceLangsJSON =
        window.localStorage.getItem(SUPPORTED_SOURCE_LANGUAGES_KEY);
    const targetLangsJSON =
        window.localStorage.getItem(SUPPORTED_TARGET_LANGUAGES_KEY);
    if (!sourceLangsJSON || !targetLangsJSON) {
      return Promise.resolve({sourceLanguages: [], targetLanguages: []});
    }

    const sourceLanguages = JSON.parse(sourceLangsJSON) as Language[];
    const targetLanguages = JSON.parse(targetLangsJSON) as Language[];
    return Promise.resolve({sourceLanguages, targetLanguages});
  }

  storeLanguages(
      locale: string, sourceLanguages: Language[],
      targetLanguages: Language[]): void {
    window.localStorage.setItem(SUPPORTED_LANGUAGES_LOCALE_KEY, locale);
    window.localStorage.setItem(
        SUPPORTED_SOURCE_LANGUAGES_KEY, JSON.stringify(sourceLanguages));
    window.localStorage.setItem(
        SUPPORTED_TARGET_LANGUAGES_KEY, JSON.stringify(targetLanguages));
    window.localStorage.setItem(
        LAST_FETCH_SUPPORTED_LANGUAGES_TIME_KEY, Date.now().toString());
  }

  private onServerLanguageListFetched(response: {
    browserLocale: string,
    sourceLanguages: Language[],
    targetLanguages: Language[],
  }): Promise<{sourceLanguages: Language[], targetLanguages: Language[]}> {
    if (response.sourceLanguages.length === 0 ||
        response.targetLanguages.length === 0) {
      return Promise.resolve({sourceLanguages: [], targetLanguages: []});
    }
    this.storeLanguages(
        response.browserLocale, response.sourceLanguages,
        response.targetLanguages);
    const sourceLanguages = response.sourceLanguages;
    const targetLanguages = response.targetLanguages;
    return Promise.resolve({sourceLanguages, targetLanguages});
  }

  private shouldFetchSupportedLanguages(): boolean {
    const languagesCacheTimeout =
        loadTimeData.getInteger('languagesCacheTimeout');
    const currentTimestamp = Date.now();

    // TODO(crbug.com/369368710): Add support for resetting cache when browser
    // locale is different.
    // If there is no stored timestamp, we should fetch supported languages.
    const storedTimestampString =
        window.localStorage.getItem(LAST_FETCH_SUPPORTED_LANGUAGES_TIME_KEY);
    if (!storedTimestampString) {
      return true;
    }

    // If the stored timestamp is stored incorrectly, we need to fetch supported
    // languages.
    const storedTimestamp = parseInt(storedTimestampString);
    if (isNaN(storedTimestamp)) {
      return true;
    }

    // If the languages are not stored at all, we should also fetch supported
    // languages.
    const sourceLanguages =
        window.localStorage.getItem(SUPPORTED_SOURCE_LANGUAGES_KEY);
    const targetLanguages =
        window.localStorage.getItem(SUPPORTED_TARGET_LANGUAGES_KEY);
    if (!sourceLanguages || !targetLanguages) {
      return true;
    }

    // Finally, if the cache is to have expired, we should fetch supported
    // languages.
    const difference = currentTimestamp - storedTimestamp;
    return (difference / languagesCacheTimeout) >= 1;
  }
}
