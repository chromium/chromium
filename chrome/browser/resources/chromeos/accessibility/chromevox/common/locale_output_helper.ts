// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides locale output services for ChromeVox, which
 * uses language information to automatically change the TTS voice.
 * Please note: we use the term 'locale' to refer to language codes e.g.
 * 'en-US'. For more information on locales:
 * https://en.wikipedia.org/wiki/Locale_(computer_software)
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {Msgs} from './msgs.js';

type AutomationNode = chrome.automation.AutomationNode;
type TtsVoice = chrome.tts.TtsVoice;

interface TextWithLocale {
  text: string;
  locale: string;
}

export class LocaleOutputHelper {
  private availableVoices_: TtsVoice[];
  private currentLocale_: string;
  private lastSpokenLocale_: string;

  static instance: LocaleOutputHelper;

  private constructor() {
    this.currentLocale_ = BROWSER_UI_LOCALE || '';
    this.lastSpokenLocale_ = this.currentLocale_;
    this.availableVoices_ = [];

    const setAvailableVoices = (): void => {
      chrome.tts.getVoices(voices => {
        this.availableVoices_ = voices || [];
      });
    };
    setAvailableVoices();
    if (window.speechSynthesis) {
      window.speechSynthesis.addEventListener(
          'voiceschanged', setAvailableVoices, /* useCapture */ false);
    }
  }

  /**
   * Computes |this.currentLocale_| and |outputString|, and returns them.
   * @param contextNode The AutomationNode that owns |text|.
   */
  computeTextAndLocale(text: string, contextNode: AutomationNode)
      : TextWithLocale {
    if (!text || !contextNode) {
      return {text, locale: BROWSER_UI_LOCALE};
    }

    // Prefer the node's detected locale and fall back on the author-assigned
    // locale.
    const nodeLocale =
        contextNode.detectedLanguage || contextNode.language || '';
    const newLocale = this.computeNewLocale_(nodeLocale);
    let outputString = text;
    const shouldAnnounce = this.shouldAnnounceLocale_(newLocale);
    if (this.hasVoiceForLocale_(newLocale)) {
      this.setCurrentLocale_(newLocale);
      if (shouldAnnounce) {
        this.lastSpokenLocale_ = newLocale;
        // Prepend the human-readable locale to |outputString|.
        const displayLanguage =
            chrome.accessibilityPrivate.getDisplayNameForLocale(
                newLocale /* Locale to translate */,
                newLocale /* Target locale */);
        outputString =
            Msgs.getMsg('language_switch', [displayLanguage, outputString]);
      }
    } else {
      // Alert the user that no voice is available for |newLocale|.
      this.setCurrentLocale_(BROWSER_UI_LOCALE);
      const displayLanguage =
          chrome.accessibilityPrivate.getDisplayNameForLocale(
              newLocale /* Locale to translate */,
              BROWSER_UI_LOCALE /* Target locale */);
      outputString =
          Msgs.getMsg('voice_unavailable_for_language', [displayLanguage]);
    }

    return {text: outputString, locale: this.currentLocale_};
  }

  private computeNewLocale_(nodeLocale: string): string {
    nodeLocale = nodeLocale.toLowerCase();
    if (LocaleOutputHelper.isValidLocale_(nodeLocale)) {
      return nodeLocale;
    }

    return BROWSER_UI_LOCALE;
  }

  private hasVoiceForLocale_(targetLocale: string): boolean {
    const components = targetLocale.split('-');
    if (!components || components.length === 0) {
      return false;
    }

    const targetLanguage = components[0];
    for (const voice of this.availableVoices_) {
      if (!voice.lang) {
        continue;
      }
      const candidateLanguage = voice.lang.toLowerCase().split('-')[0];
      if (candidateLanguage === targetLanguage) {
        return true;
      }
    }

    return false;
  }

  private setCurrentLocale_(locale: string): void {
    if (LocaleOutputHelper.isValidLocale_(locale)) {
      this.currentLocale_ = locale;
    }
  }

  private shouldAnnounceLocale_(newLocale: string): boolean {
    const [lastSpokenLanguage, lastSpokenCountry] =
        this.lastSpokenLocale_.split('-');
    const [newLanguage, newCountry] = newLocale.split('-');
    if (lastSpokenLanguage !== newLanguage) {
      return true;
    }

    if (!newCountry) {
      // If |newCountry| is undefined, then we don't want to announce the
      // locale. For example, we don't want to announce 'en-us' -> 'en'.
      return false;
    }

    return lastSpokenCountry !== newCountry;
  }

  // =============== Static Methods ==============

  /** Creates a singleton instance of LocaleOutputHelper. */
  static init(): void {
    if (LocaleOutputHelper.instance !== undefined) {
      throw new Error(
          'LocaleOutputHelper is a singleton, can only initialize once');
    }

    LocaleOutputHelper.instance = new LocaleOutputHelper();
  }

  private static isValidLocale_(locale: string): boolean {
    return chrome.accessibilityPrivate.getDisplayNameForLocale(
               locale, locale) !== '';
  }
}

// Local to module.

const BROWSER_UI_LOCALE = chrome.i18n.getUILanguage().toLowerCase();

TestImportManager.exportForTesting(LocaleOutputHelper);
