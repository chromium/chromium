// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** A class that provides TTS test support for crosier tests. */
class GoogleTtsTestSupport {
  constructor() {
    this.init_();
    this.notifyCcFixture_();
  }

  /** @private */
  init_() {
    window.testUtterances = [];
    // When `isTastTest_` is true, Google TTS will push spoken utterances
    // onto `window.testUtterances`. This allows us to verify spoken
    // utterances from tests.
    window.engine.isTastTest_ = true;
  }

  /**
   * Notifies the SpokenFeedbackIntegrationTest fixture, which waits for the JS
   * side to call `chrome.test.sendScriptResult`, that it can continue.
   * @private
   */
  notifyCcFixture_() {
    chrome.test.sendScriptResult('ready');
  }

  /** Ensures that Google TTS is loaded and ready for testing. */
  async ensureLoaded() {
    // The Google TTS engine is fully loaded if it contains at least one voice
    // with the Google TTS extension ID.
    const loaded = (voices) => {
      for (const voice of voices) {
        if (voice.extensionId === 'gjjabgpgjpampikjhjpfhneeoapjbjaf') {
          return true;
        }
      }

      return false;
    };

    await new Promise(resolve => {
      const intervalId = setInterval(() => {
        chrome.tts.getVoices((voices) => {
          if (loaded(voices)) {
            clearInterval(intervalId);
            resolve();
          }
        });
      }, 300);
    });

    this.notifyCcFixture_();
  }

  /**
   * Requests speech from the Google TTS engine.
   * @param {string} utterance
   */
  speak(utterance) {
    chrome.tts.speak(utterance, {volume: 0.01});
    this.notifyCcFixture_();
  }

  /**
   * Loops through the list of spoken strings to see if the provided utterance
   * has been spoken. Discards spoken utterances that don't match the provided
   * one. Returns once the utterance has been found in the list of spoken
   * strings.
   * @param {string} utterance
   */
  async consume(utterance) {
    const hasSpokenUtterance = () => {
      const data = window.testUtterances.shift();
      if (!data) {
        return false;
      }

      return utterance === data.utterance;
    };

    if (hasSpokenUtterance()) {
      this.notifyCcFixture_();
      return;
    }

    await new Promise(resolve => {
      const intervalId = setInterval(() => {
        if (hasSpokenUtterance()) {
          clearInterval(intervalId);
          resolve();
        }
      }, 300);
    });
    this.notifyCcFixture_();
  }
}

window.googleTtsTestSupport = new GoogleTtsTestSupport();
