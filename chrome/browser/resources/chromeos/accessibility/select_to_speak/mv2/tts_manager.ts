// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';

/**
 * The wrapper for Select-to-speak's text-to-speech features.
 */
export class TtsManager {
  private clientTtsOptions_: chrome.tts.TtsOptions;
  private currentCharIndex_: number;
  private fallbackVoice_: string | undefined;
  private isNetworkVoice_: boolean;
  private isSpeaking_: boolean;
  private pauseCompleteCallback_: (() => void) | null;
  private text_: string | null;

  /** Please keep fields in alphabetical order. */
  constructor() {
    /**
     * The TTS options that the client passed in.
     */
    this.clientTtsOptions_ = {};

    /**
     * The current char index to the |this.text_| indicating the current spoken
     * word. For example, if |this.text_| is "hello world" and TTS is speaking
     * the second word, the |this.currentCharIndex_| should be 6.
     */
    this.currentCharIndex_ = 0;

    /**
     * The fallback voice to use if TTS fails.
     */
    this.fallbackVoice_ = undefined;

    /**
     * Whether the last TTS request was made with a network voice.
     */
    this.isNetworkVoice_ = false;

    /**
     * Whether TTS is speaking.
     */
    this.isSpeaking_ = false;

    /**
     * Function to be called when STS finishes a pausing request.
     */
    this.pauseCompleteCallback_ = null;

    /**
     * The text currently being spoken.
     */
    this.text_ = null;
  }

  /**
   * Whether TTS is speaking. If TTS is paused, this will return false.
   */
  isSpeaking(): boolean {
    return this.isSpeaking_;
  }

  /**
   * Sets TtsManager with the parameters and starts reading |text|.
   * @param text The text to read.
   * @param ttsOptions The options for TTS.
   * @param networkVoice Whether a network voice is specified for TTS.
   * @param fallbackVoice A voice to use if to retry if TTS
   *     fails.
   */
  speak(text: string, ttsOptions: chrome.tts.TtsOptions,
        networkVoice: boolean, fallbackVoice?: string): void {
    // @ts-ignore: TODO(b/): Change to `enqueue`.
    if (ttsOptions.enqueued) {
      console.warn('TtsManager does not support a queue of utterances.');
      return;
    }
    this.cleanTtsState_();
    this.text_ = text;
    this.isNetworkVoice_ = networkVoice;
    this.fallbackVoice_ = fallbackVoice;
    this.startSpeakingTextWithOffset_(0, false /* resume */, ttsOptions);
  }

  /**
   * Starts reading text with |offset|.
   * @param offset The character offset into the text at which to start
   *     speaking.
   * @param resume Whether it is a resume action.
   * @param ttsOptions The options for TTS.
   */
  private startSpeakingTextWithOffset_(offset: number, resume: boolean,
        ttsOptions: chrome.tts.TtsOptions): void {
    // @ts-ignore: TODO(b/270623046): this.text_ can be null.
    const text = this.text_.slice(offset);
    const modifiedOptions = Object.assign({}, ttsOptions);
    // Saves a copy of the ttsOptions for resume.
    Object.assign(this.clientTtsOptions_, ttsOptions);
    modifiedOptions.onEvent = event => {
      switch (event.type) {
        case chrome.tts.EventType.ERROR:
          if (this.isNetworkVoice_) {
            // Retry with local voice. Use modifiedOptions to preserve
            // word and character indices.
            console.warn('Network TTS error, retrying with local voice');
            const localOptions = Object.assign({}, modifiedOptions);
            localOptions.voiceName = this.fallbackVoice_;
            if (this.text_) {
              this.speak(
                  this.text_, localOptions, /*networkVoice=*/ false, undefined);
            }
          }
          break;
        case chrome.tts.EventType.START:
          this.isSpeaking_ = true;
          // Find the first non-space char index in text, or 0 if the text is
          // null or the first char is non-space.
          this.currentCharIndex_ = (text || '').search(/\S|$/) + offset;
          if (resume) {
            TtsManager.sendEventToOptions(ttsOptions, {
              type: chrome.tts.EventType.RESUME,
              charIndex: this.currentCharIndex_,
            });
            break;
          }
          TtsManager.sendEventToOptions(ttsOptions, {
            type: chrome.tts.EventType.START,
            charIndex: this.currentCharIndex_,
          });
          break;
        case chrome.tts.EventType.END:
          this.isSpeaking_ = false;
          this.currentCharIndex_ = text.length + offset;
          TtsManager.sendEventToOptions(ttsOptions, {
            type: chrome.tts.EventType.END,
            charIndex: this.currentCharIndex_,
          });
          break;
        case chrome.tts.EventType.WORD:
          this.isSpeaking_ = true;
          // @ts-ignore: TODO(b/270623046): event.charIndex can be undefined.
          this.currentCharIndex_ = event.charIndex + offset;
          TtsManager.sendEventToOptions(ttsOptions, {
            type: chrome.tts.EventType.WORD,
            charIndex: this.currentCharIndex_,
            length: event.length,
          });
          break;
        case chrome.tts.EventType.INTERRUPTED:
        case chrome.tts.EventType.CANCELLED:
          this.isSpeaking_ = false;
          // Checks |this.pauseCompleteCallback_| as a proxy to see if the
          // interrupted events are from |this.pause()|.
          if (this.pauseCompleteCallback_) {
            TtsManager.sendEventToOptions(ttsOptions, {
              type: chrome.tts.EventType.PAUSE,
              charIndex: this.currentCharIndex_,
            });
            this.pauseCompleteCallback_();
            break;
          }
          TtsManager.sendEventToOptions(ttsOptions, event);
          break;
        // Passes other events directly.
        default:
          TtsManager.sendEventToOptions(ttsOptions, event);
          break;
      }
    };
    chrome.tts.speak(text, modifiedOptions);
  }

  /**
   * Pause the TTS. The chrome.tts.pause method is not fully supported by all
   * TTS engines so we mock the logic using chrome.tts.stop. This function also
   * sets the |this.pauseCompleteCallback_|, which will be executed at the end
   * of the pause process in TTS. This enables us to execute functions when the
   * pause request is finished. For example, to navigate the next sentence, we
   * trigger pause_ and start finding the next sentence when the pause function
   * is fulfilled.
   */
  pause(): Promise<void> {
    return new Promise<void>(resolve => {
      this.pauseCompleteCallback_ = () => {
        this.pauseCompleteCallback_ = null;
        resolve();
      };
      chrome.tts.stop();
    });
  }

  /**
   * Resumes the TTS.
   * @param ttsOptions The options for TTS. If this is not passed,
   *     the previous options will be used.
   */
  resume(ttsOptions?: chrome.tts.TtsOptions): void {
    ttsOptions = ttsOptions || this.clientTtsOptions_;
    // If TTS is speaking now, returns immediately.
    if (this.isSpeaking_) {
      return;
    }
    // If there is no content in the remaining text, sends an error message and
    // returns early. This avoids sending 'end' events to client.
    // @ts-ignore: TODO(b/270623046): this.text_ can be null.
    if (this.text_.slice(this.currentCharIndex_).trim().length === 0) {
      TtsManager.sendEventToOptions(ttsOptions, {
        type: chrome.tts.EventType.ERROR,
        errorMessage: TtsManager.ErrorMessage.RESUME_WITH_EMPTY_CONTENT,
      });
      return;
    }
    this.startSpeakingTextWithOffset_(
        this.currentCharIndex_, true /* resume */, ttsOptions);
  }

  /**
   * Stops the TTS.
   */
  stop(): void {
    chrome.tts.stop();
  }

  private cleanTtsState_() : void {
    this.text_ = null;
    this.clientTtsOptions_ = {};
    this.currentCharIndex_ = 0;
    this.pauseCompleteCallback_ = null;
    this.isSpeaking_ = false;
    this.isNetworkVoice_ = false;
  }

  /**
   * Sends TtsEvent to TtsOptions.
   * @param options
   * @param event
   */
  static sendEventToOptions(options: chrome.tts.TtsOptions,
        event: chrome.tts.TtsEvent): void {
    if (options.onEvent) {
      options.onEvent(event);
      return;
    }
    console.warn('onEvent is not defined in the TtsOptions');
  }
}

export namespace TtsManager {
  /**
  * Error message for "error" events.
  */
  export enum ErrorMessage {
    RESUME_WITH_EMPTY_CONTENT = 'Cannot resume with empty content.',
  }
}

TestImportManager.exportForTesting(TtsManager);
