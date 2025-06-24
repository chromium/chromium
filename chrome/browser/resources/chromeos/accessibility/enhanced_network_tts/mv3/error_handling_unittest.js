// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['enhanced_network_tts_e2e_test_base.js']);

/**
 * Test fixture for error_handling_unittest.js.
 */
EnhancedNetworkTtsErrorHandlingUnitTest =
    class extends EnhancedNetworkTE2ETestBase {
  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    super.testGenPreambleCommon(
        'kEnhancedNetworkTtsExtensionId', false /* failOnConsoleError */);
  }
};

SYNC_TEST_F(
    'EnhancedNetworkTtsErrorHandlingUnitTest',
    'onSpeakWithAudioStreamEventMojoPrivateFailed', async function() {
      // This makes chrome.mojoPrivate.requireAsync(string) returns a failed
      // promise.
      chrome.mojoPrivate.unregisterAllModuleForTesting();

      const utterance = 'test';
      const options = {'voiceName': 'Enhanced TTS English (Australian Accent)'};
      const audioStreamOptions = {'bufferSize': 10000, 'sampleRate': 22000};
      const sendTtsAudio = receivedBuffer => {
        throw new Error('Assertion failed: does not expect incoming buffer.');
      };
      const sendError = error => {
        assertEquals(error, 'Error: unable to get mojoPrivate bindings');
      };

      await enhancedNetworkTts.onSpeakWithAudioStreamEvent(
          utterance, options, audioStreamOptions, sendTtsAudio, sendError);
    });

SYNC_TEST_F(
    'EnhancedNetworkTtsErrorHandlingUnitTest',
    'onSpeakWithAudioStreamEventErrorCodeReceived', async function() {
      // Prepare the mockTtsApi to respond with error code 1.
      const mockTtsApi = MockTtsApi;
      mockTtsApi.enqueueErrorCode(
          ash.enhancedNetworkTts.mojom.TtsRequestError.kOverLength);
      chrome.mojoPrivate.registerMockedModuleForTesting(
          'ash.enhanced_network_tts', mockTtsApi);

      const utterance = 'test';
      const options = {'voiceName': 'Enhanced TTS English (Australian Accent)'};
      const audioStreamOptions = {'bufferSize': 10000, 'sampleRate': 22000};
      const sendTtsAudio = receivedBuffer => {
        throw new Error('Assertion failed: does not expect incoming buffer.');
      };
      const sendError = error => {
        assertEquals(error, 'Error: utterance too long');
      };

      await enhancedNetworkTts.onSpeakWithAudioStreamEvent(
          utterance, options, audioStreamOptions, sendTtsAudio, sendError);
    });

SYNC_TEST_F(
    'EnhancedNetworkTtsErrorHandlingUnitTest', 'DecodeAudioDataFailed',
    async function() {
      sampleRate = 10000;
      audioBuffer = await EnhancedNetworkTts.decodeAudioDataAtSampleRate(
          new Uint8Array([]).buffer, sampleRate);
      assertEquals(audioBuffer, null);
    });
