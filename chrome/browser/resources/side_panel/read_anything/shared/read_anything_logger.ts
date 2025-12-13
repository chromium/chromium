// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isEspeak, isNatural} from '../read_aloud/voice_language_conversions.js';

import {MetricsBrowserProxyImpl, ReadAnythingSpeechError, ReadAnythingVoiceType} from './metrics_browser_proxy.js';
import type {MetricsBrowserProxy, ReadAloudSettingsChange, ReadAnythingSettingsChange} from './metrics_browser_proxy.js';

export enum TimeFrom {
  APP = 'App',
  TOOLBAR = 'Toolbar',
}

export enum SpeechControls {
  PLAY = 'Play',
  PAUSE = 'Pause',
  NEXT = 'NextButton',
  PREVIOUS = 'PreviousButton',
}

// Handles the business logic for logging.
export class ReadAnythingLogger {
  private metrics: MetricsBrowserProxy = MetricsBrowserProxyImpl.getInstance();

  logEmptyState() {
    this.metrics.recordEmptyState();
  }

  logSpeechStopSource(source: number) {
    this.metrics.recordSpeechStopSource(source);
  }

  logSpeechError(errorCode: string) {
    let error: ReadAnythingSpeechError;
    switch (errorCode) {
      case 'text-too-long':
        error = ReadAnythingSpeechError.TEXT_TOO_LONG;
        break;
      case 'voice-unavailable':
        error = ReadAnythingSpeechError.VOICE_UNAVAILABE;
        break;
      case 'language-unavailable':
        error = ReadAnythingSpeechError.LANGUAGE_UNAVAILABLE;
        break;
      case 'invalid-argument':
        error = ReadAnythingSpeechError.INVALID_ARGUMENT;
        break;
      case 'synthesis-failed':
        error = ReadAnythingSpeechError.SYNTHESIS_FAILED;
        break;
      case 'synthesis-unavailable':
        error = ReadAnythingSpeechError.SYNTHESIS_UNVAILABLE;
        break;
      case 'audio-busy':
        error = ReadAnythingSpeechError.AUDIO_BUSY;
        break;
      case 'audio-hardware':
        error = ReadAnythingSpeechError.AUDIO_HARDWARE;
        break;
      case 'network':
        error = ReadAnythingSpeechError.NETWORK;
        break;
      default:
        return;
    }

    // There are more error code possibilities, but right now, we only care
    // about tracking the above error codes.
    this.metrics.recordSpeechError(error);
  }

  logTimeFrom(from: TimeFrom, startTime: number, endTime: number) {
    const umaName = 'Accessibility.ReadAnything.' +
        'TimeFrom' + from + 'StartedToConstructor';
    this.metrics.recordTime(umaName, endTime - startTime);
  }

  logNewPage(speechPlayed: boolean) {
    speechPlayed ? this.metrics.recordNewPageWithSpeech() :
                   this.metrics.recordNewPage();
  }

  logHighlightGranularity(highlight: number) {
    this.metrics.recordHighlightGranularity(highlight);
  }

  private logVoiceTypeUsedForReading_(voice: SpeechSynthesisVoice|null) {
    if (!voice) {
      return;
    }

    let voiceType: ReadAnythingVoiceType;
    if (isNatural(voice)) {
      voiceType = ReadAnythingVoiceType.NATURAL;
    } else if (isEspeak(voice)) {
      voiceType = ReadAnythingVoiceType.ESPEAK;
    } else {
      // <if expr="is_chromeos">
      voiceType = ReadAnythingVoiceType.CHROMEOS;
      // </if>
      // <if expr="not is_chromeos">
      voiceType = ReadAnythingVoiceType.SYSTEM;

      // When a system voice is used, log additional information to better
      // understand the TTS engine state when the system voice is used.
      // Extension state information cannot easily be passed to the renderer,
      // so this logging needs to be handled within the page handler.
      this.metrics.recordExtensionState();
      // </if>
    }

    this.metrics.recordVoiceType(voiceType);
  }

  private logLanguageUsedForReading_(lang: string|undefined) {
    if (!lang) {
      return;
    }

    // See tools/metrics/histograms/enums.xml enum LocaleCodeBCP47. The enum
    // there doesn't always have locales where the base lang and the locale
    // are the same (e.g. they don't have id-id, but do have id). So if the
    // base lang and the locale are the same, just use the base lang.
    let langToLog = lang;
    const langSplit = lang.toLowerCase().split('-');
    if (langSplit.length === 2 && langSplit[0]! === langSplit[1]!) {
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

  logSpeechPlaySession(startTime: number, voice: SpeechSynthesisVoice|null) {
    this.logVoiceTypeUsedForReading_(voice);
    this.logLanguageUsedForReading_(voice?.lang);
    this.metrics.recordSpeechPlaybackLength(Date.now() - startTime);
  }

  logSpeechControlClick(control: SpeechControls) {
    this.metrics.incrementMetricCount(
        'Accessibility.ReadAnything.ReadAloud' + control + 'SessionCount');
  }

  static getInstance(): ReadAnythingLogger {
    return instance || (instance = new ReadAnythingLogger());
  }

  static setInstance(obj: ReadAnythingLogger) {
    instance = obj;
  }
}

let instance: ReadAnythingLogger|null = null;
