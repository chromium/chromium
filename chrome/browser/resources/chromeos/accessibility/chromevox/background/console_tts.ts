// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A TTS engine that writes to globalThis.console.
 */
import {SpeechLog} from '../common/log_types.js';
import type {QueueMode, TtsSpeechProperties} from '../common/tts_types.js';
import {TtsCategory} from '../common/tts_types.js';

import {LogStore} from './logging/log_store.js';
import {ChromeVoxPrefs} from './prefs.js';
import type {TtsCapturingEventListener, TtsInterface} from './tts_interface.js';


export class ConsoleTts implements TtsInterface {
  /**
   * True if the console TTS is enabled by the user.
   */
  private enabled_: boolean;

  constructor() {
    this.enabled_ = ChromeVoxPrefs.instance?.getPrefs()['enableSpeechLogging'];
  }

  speak(
      textString: string, queueMode: QueueMode,
      properties?: TtsSpeechProperties): ConsoleTts {
    if (this.enabled_ && globalThis.console) {
      const category = properties?.category ?? TtsCategory.NAV;

      const speechLog = new SpeechLog(textString, queueMode, category);
      LogStore.instance.writeLog(speechLog);
      console.log(speechLog.toString());
    }
    return this;
  }

  isSpeaking(): boolean {
    return false;
  }

  stop(): void {
    if (this.enabled_) {
      console.log('Stop');
    }
  }

  // @ts-ignore Unread value.
  override addCapturingEventListener(_listener: TtsCapturingEventListener):
      void {}

  // @ts-ignore Unread value.
  override removeCapturingEventListener(_listener: TtsCapturingEventListener):
      void {}

  // @ts-ignore Unread value.
  override increaseOrDecreaseProperty(
      _propertyName: string, _increase: boolean): void {}

  // @ts-ignore Unread value.
  override setProperty(_propertyName: string, _value: number): void {}


  // @ts-ignore Unread value.
  override propertyToPercentage(_property: string): number|null {
    return null;
  }

  /**
   * Sets the enabled bit.
   * @param enabled The new enabled bit.
   */
  setEnabled(enabled: boolean) {
    this.enabled_ = enabled;
  }

  // @ts-ignore Unread value.
  override getDefaultProperty(_property: string): number {
    return 0;
  }

  // @ts-ignore Unread value.
  override toggleSpeechOnOrOff(): boolean {
    return true;
  }
}
