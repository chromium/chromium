// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MetricsBrowserProxyImpl, ReadAnythingVoiceType} from './metrics_browser_proxy.js';
import type {MetricsBrowserProxy, ReadAloudSettingsChange, ReadAnythingSettingsChange} from './metrics_browser_proxy.js';
import {isEspeak, isNatural} from './voice_language_util.js';

// Handles the business logic for logging.
export class ReadAnythingLogger {
  private metrics: MetricsBrowserProxy = MetricsBrowserProxyImpl.getInstance();

  logNewPage(speechPlayed: boolean) {
    speechPlayed ? this.metrics.recordNewPageWithSpeech() :
                   this.metrics.recordNewPage();
  }

  logHighlightState(highlightOn: boolean) {
    highlightOn ? this.metrics.recordHighlightOn() :
                  this.metrics.recordHighlightOff();
  }

  // <if expr="chromeos_ash">
  logVoiceTypeUsedForReading(voice: SpeechSynthesisVoice|undefined) {
    if (!voice) {
      return;
    }

    let voiceType: ReadAnythingVoiceType|undefined;
    if (isNatural(voice)) {
      voiceType = ReadAnythingVoiceType.NATURAL;
    } else if (isEspeak(voice)) {
      voiceType = ReadAnythingVoiceType.ESPEAK;
    } else {
      voiceType = ReadAnythingVoiceType.CHROMEOS;
    }

    this.metrics.recordVoiceType(voiceType);
  }
  // </if>

  logLanguageUsedForReading(lang: string|undefined) {
    if (!lang) {
      return;
    }

    // See tools/metrics/histograms/enums.xml enum LocaleCodeISO639. The enum
    // there doesn't always have locales where the base lang and the locale
    // are the same (e.g. they don't have id-id, but do have id). So if the
    // base lang and the locale are the same, just use the base lang.
    let langToLog = lang;
    const langSplit = lang.toLowerCase().split('-');
    if (langSplit.length === 2 && langSplit[0] === langSplit[1]) {
      langToLog = langSplit[0];
    }
    this.metrics.recordLanguage(langToLog);
  }

  logTextSettingsChange(settingsChange: ReadAnythingSettingsChange) {
    this.metrics.recordTextSettingsChange(settingsChange);
  }

  logSpeechSettingsChange(settingsChange: ReadAloudSettingsChange) {
    this.metrics.recordSpeechSettingsChange(settingsChange);
  }

  logVoiceSpeed(index: number) {
    this.metrics.recordVoiceSpeed(index);
  }

  static getInstance(): ReadAnythingLogger {
    return instance || (instance = new ReadAnythingLogger());
  }
}

let instance: ReadAnythingLogger|null = null;
