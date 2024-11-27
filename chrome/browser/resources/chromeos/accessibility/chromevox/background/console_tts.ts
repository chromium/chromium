// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A TTS engine that writes to globalThis.console.
 */
import {SpeechLog} from '../common/log_types.js';
import {QueueMode, TtsCategory, TtsSpeechProperties} from '../common/tts_types.js';

import {LogStore} from './logging/log_store.js';
import {ChromeVoxPrefs} from './prefs.js';
import {TtsCapturingEventListener, TtsInterface} from './tts_interface.js';


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
  override addCapturingEventListener(listener: TtsCapturingEventListener):
      void {}

  // @ts-ignore Unread value.
  override removeCapturingEventListener(listener: TtsCapturingEventListener):
      void {}

  // @ts-ignore Unread value.
  override increaseOrDecreaseProperty(propertyName: string, increase: boolean):
      void {}

  // @ts-ignore Unread value.
  override setProperty(propertyName: string, value: number): void {}


  // @ts-ignore Unread value.
  override propertyToPercentage(property: string): number|null {
    return null;
  }

  /**
   * Sets the enabled bit.
   * @param {boolean} enabled The new enabled bit.
   */
  setEnabled(enabled: boolean) {
    this.enabled_ = enabled;
  }

  // @ts-ignore Unread value.
  override getDefaultProperty(property: string): number {
    return 0;
  }

  // @ts-ignore Unread value.
  override toggleSpeechOnOrOff(): boolean {
    return true;
  }
}
