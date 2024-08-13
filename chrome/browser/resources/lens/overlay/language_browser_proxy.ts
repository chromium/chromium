// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A browser proxy for fetching language settings.
 */
let instance: LanguageBrowserProxy|null = null;

export interface LanguageBrowserProxy {
  getLanguageList(): Promise<chrome.languageSettingsPrivate.Language[]>;
  getTranslateTargetLanguage(): Promise<string>;
}

export class LanguageBrowserProxyImpl implements LanguageBrowserProxy {
  static getInstance(): LanguageBrowserProxy {
    return instance || (instance = new LanguageBrowserProxyImpl());
  }

  static setInstance(obj: LanguageBrowserProxy) {
    instance = obj;
  }

  getLanguageList(): Promise<chrome.languageSettingsPrivate.Language[]> {
    return chrome.languageSettingsPrivate.getLanguageList();
  }

  getTranslateTargetLanguage(): Promise<string> {
    return chrome.languageSettingsPrivate.getTranslateTargetLanguage();
  }
}
