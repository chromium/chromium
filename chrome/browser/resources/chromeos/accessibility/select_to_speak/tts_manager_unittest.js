// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['select_to_speak_e2e_test_base.js']);
GEN_INCLUDE(['../common/testing/mock_tts.js']);

/** Mock TTS client. */
class MockTtsClient {
  constructor() {
    this.receivedEvent;
  }

  getTtsOptions() {
    const options = /** @type {!chrome.tts.TtsOptions} */ ({});
    options.onEvent = event => this.receivedEvent = event;
    return options;
  }
}

/**
 * Test fixture for tts_manager.js.
 */
SelectToSpeakTtsManagerUnitTest = class extends SelectToSpeakE2ETest {
  constructor() {
    super();
    this.mockTts = new MockTts();
    chrome.tts = this.mockTts;
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    this.mockTtsClient = new MockTtsClient();
    this.ttsManager = new TtsManager();
  }
};

AX_TEST_F('SelectToSpeakTtsManagerUnitTest', 'SpeakUtterance', function() {
  this.ttsManager.speak(
      ' text with space ', this.mockTtsClient.getTtsOptions());
  const receivedEvent = this.mockTtsClient.receivedEvent;
  assertEquals(receivedEvent.type, chrome.tts.EventType.START);
  assertEquals(receivedEvent.charIndex, 1);
  assertTrue(this.ttsManager.isSpeaking());
});

AX_TEST_F('SelectToSpeakTtsManagerUnitTest', 'StopUtterance', function() {
  this.ttsManager.speak(
      ' text with space ', this.mockTtsClient.getTtsOptions());
  this.ttsManager.stop();
  const receivedEvent = this.mockTtsClient.receivedEvent;
  assertEquals(receivedEvent.type, chrome.tts.EventType.INTERRUPTED);
  assertFalse(this.ttsManager.isSpeaking());
});

AX_TEST_F('SelectToSpeakTtsManagerUnitTest', 'FinishUtterance', function() {
  this.ttsManager.speak(
      ' text with space ', this.mockTtsClient.getTtsOptions());

  // Let TTS finish all the utterance.
  this.mockTts.finishPendingUtterance();

  const receivedEvent = this.mockTtsClient.receivedEvent;
  assertEquals(receivedEvent.type, chrome.tts.EventType.END);
  // The ending index will be the length of the entire string.
  assertEquals(receivedEvent.charIndex, 17);
  assertFalse(this.ttsManager.isSpeaking());
});

AX_TEST_F(
    'SelectToSpeakTtsManagerUnitTest', 'SendWordEventsWhenSpeaking',
    function() {
      this.ttsManager.speak(
          ' text with space ', this.mockTtsClient.getTtsOptions());

      // Let TTS finish the first word.
      this.mockTts.speakUntilCharIndex(6);
      let receivedEvent = this.mockTtsClient.receivedEvent;
      assertEquals(receivedEvent.type, chrome.tts.EventType.WORD);
      assertEquals(receivedEvent.charIndex, 6);
      assertTrue(this.ttsManager.isSpeaking());

      // Let TTS finish the second word.
      this.mockTts.speakUntilCharIndex(11);
      receivedEvent = this.mockTtsClient.receivedEvent;
      assertEquals(receivedEvent.type, chrome.tts.EventType.WORD);
      assertEquals(receivedEvent.charIndex, 11);
      assertTrue(this.ttsManager.isSpeaking());
    });

AX_TEST_F('SelectToSpeakTtsManagerUnitTest', 'PauseAndResume', function() {
  const options = this.mockTtsClient.getTtsOptions();
  options.rate = 0.5;
  this.ttsManager.speak(' text with space ', options);

  this.mockTts.speakUntilCharIndex(6);
  assertTrue(this.ttsManager.isSpeaking());
  assertEquals(this.mockTts.getOptions().rate, 0.5);
  assertEquals(this.mockTts.pendingUtterances()[0], ' text with space ');

  // Pause will stop speaking.
  this.ttsManager.pause();
  let receivedEvent = this.mockTtsClient.receivedEvent;
  assertEquals(receivedEvent.type, chrome.tts.EventType.PAUSE);
  assertEquals(receivedEvent.charIndex, 6);
  assertFalse(this.ttsManager.isSpeaking());

  // Resume will generate new utterance for tts engine.
  this.ttsManager.resume();
  receivedEvent = this.mockTtsClient.receivedEvent;
  assertEquals(receivedEvent.type, chrome.tts.EventType.RESUME);
  assertEquals(receivedEvent.charIndex, 6);
  assertTrue(this.ttsManager.isSpeaking());
  assertEquals(this.mockTts.getOptions().rate, 0.5);
  assertEquals(this.mockTts.pendingUtterances()[0], 'with space ');

  // Finish the next word and pause.
  this.mockTts.speakUntilCharIndex(5);
  this.ttsManager.pause();
  receivedEvent = this.mockTtsClient.receivedEvent;
  assertEquals(receivedEvent.type, chrome.tts.EventType.PAUSE);
  assertEquals(receivedEvent.charIndex, 11);
  assertFalse(this.ttsManager.isSpeaking());

  // Resume with a different rate.
  options.rate = 1.5;
  this.ttsManager.resume(options);
  receivedEvent = this.mockTtsClient.receivedEvent;
  assertEquals(receivedEvent.type, chrome.tts.EventType.RESUME);
  assertEquals(receivedEvent.charIndex, 11);
  assertTrue(this.ttsManager.isSpeaking());
  assertEquals(this.mockTts.getOptions().rate, 1.5);
  assertEquals(this.mockTts.pendingUtterances()[0], 'space ');
});

AX_TEST_F(
    'SelectToSpeakTtsManagerUnitTest', 'ResumeWithNoRemainingContent',
    function() {
      const options = this.mockTtsClient.getTtsOptions();
      this.ttsManager.speak(' text ', options);

      this.mockTts.speakUntilCharIndex(5);
      assertTrue(this.ttsManager.isSpeaking());
      assertEquals(this.mockTts.pendingUtterances()[0], ' text ');

      // Pause will stop speaking.
      this.ttsManager.pause();
      assertFalse(this.ttsManager.isSpeaking());

      // Resume will trigger an error event.
      this.ttsManager.resume();
      const receivedEvent = this.mockTtsClient.receivedEvent;
      assertEquals(receivedEvent.type, chrome.tts.EventType.ERROR);
      assertEquals(
          receivedEvent.errorMessage,
          TtsManager.ErrorMessage.RESUME_WITH_EMPTY_CONTENT);
      assertFalse(this.ttsManager.isSpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);
    });
