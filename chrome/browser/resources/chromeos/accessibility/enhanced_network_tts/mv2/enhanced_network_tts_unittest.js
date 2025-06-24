// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['enhanced_network_tts_e2e_test_base.js']);

/**
 * Test fixture for enhanced_network_tts_unittest.js.
 */
EnhancedNetworkTtsUnitTest = class extends EnhancedNetworkTE2ETestBase {
  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    super.testGenPreambleCommon(
        'kEnhancedNetworkTtsExtensionId', true /* failOnConsoleError */);
  }
};

SYNC_TEST_F(
    'EnhancedNetworkTtsUnitTest', 'onSpeakWithAudioStreamEventSucceed',
    async function() {
      // Prepare the mockTtsApi to respond with audio data that corresponds to a
      // 0.2135s playback containing "Hello world".
      const mockTtsApi = MockTtsApi;
      mockTtsApi.enqueueAudioData(
          generateTestBufferData(), generateTestTimeInfoData(),
          true /* lastData */);
      chrome.mojoPrivate.registerMockedModuleForTesting(
          'ash.enhanced_network_tts', mockTtsApi);

      const utterance = 'test';
      const options = {'voiceName': 'Enhanced TTS English (Australian Accent)'};
      const sampleRate = 10000;
      const bufferSize = 400;
      const audioStreamOptions = {bufferSize, sampleRate};
      const decodedAudioData =
          await EnhancedNetworkTts.decodeAudioDataAtSampleRate(
              generateTestBufferData(), sampleRate);
      // Each buffer corresponds to 0.04s.
      const expectedBuffers = [
        // The first 0.04s is empty.
        {
          audioBuffer:
              EnhancedNetworkTts.subarrayFrom(decodedAudioData, 0, bufferSize),
          charIndex: undefined,
          isLastBuffer: false,
        },
        // 0.04s - 0.08s contains the start of the first word.
        {
          audioBuffer: EnhancedNetworkTts.subarrayFrom(
              decodedAudioData, bufferSize, bufferSize),
          charIndex: 0,
          isLastBuffer: false,
        },
        // 0.08s - 0.12s is a part of the first word.
        {
          audioBuffer: EnhancedNetworkTts.subarrayFrom(
              decodedAudioData, bufferSize * 2, bufferSize),
          charIndex: undefined,
          isLastBuffer: false,
        },
        // 0.12s - 0.16s contains the end of the first word and the start of the
        // second.
        {
          audioBuffer: EnhancedNetworkTts.subarrayFrom(
              decodedAudioData, bufferSize * 3, bufferSize),
          charIndex: 6,
          isLastBuffer: false,
        },
        // 0.16s - 0.2s is a part of the second word.
        {
          audioBuffer: EnhancedNetworkTts.subarrayFrom(
              decodedAudioData, bufferSize * 4, bufferSize),
          charIndex: undefined,
          isLastBuffer: false,
        },
        // The last buffer runs 0.0135s.
        {
          audioBuffer: EnhancedNetworkTts.subarrayFrom(
              decodedAudioData, bufferSize * 5, bufferSize),
          charIndex: undefined,
          isLastBuffer: true,
        },
      ];
      const sendTtsAudio = function(receivedBuffer) {
        const expectedBuffer = expectedBuffers.shift();
        assertEqualsJSON(expectedBuffer, receivedBuffer);
      };

      await enhancedNetworkTts.onSpeakWithAudioStreamEvent(
          utterance, options, audioStreamOptions, sendTtsAudio);
    });

SYNC_TEST_F('EnhancedNetworkTtsUnitTest', 'GenerateRequest', async function() {
  let utterance = 'name and lang should be specified together';
  let options = {voiceName: 'test name', lang: 'en'};
  let request = EnhancedNetworkTts.generateRequest(utterance, options);
  assertEqualsJSON(
      request, {utterance, rate: 1.0, voice: 'test name', lang: 'en'});

  utterance = 'name without lang will be ignored';
  options = {voiceName: 'test name'};
  request = EnhancedNetworkTts.generateRequest(utterance, options);
  assertEqualsJSON(
      request, {utterance, rate: 1.0, voice: undefined, lang: undefined});

  utterance = 'lang without name will be ignored';
  options = {lang: 'en'};
  request = EnhancedNetworkTts.generateRequest(utterance, options);
  assertEqualsJSON(
      request, {utterance, rate: 1.0, voice: undefined, lang: undefined});

  utterance = 'only lang code (e.g., en) will be used';
  options = {voiceName: 'test name', lang: 'en_US'};
  request = EnhancedNetworkTts.generateRequest(utterance, options);
  assertEqualsJSON(
      request, {utterance, rate: 1.0, voice: 'test name', lang: 'en'});

  utterance = 'utterance without options can proceed';
  options = {};
  request = EnhancedNetworkTts.generateRequest(utterance, options);
  assertEqualsJSON(
      request, {utterance, rate: 1.0, voice: undefined, lang: undefined});

  utterance = 'Rate will be sent along with the request';
  options = {rate: 3.0};
  request = EnhancedNetworkTts.generateRequest(utterance, options);
  assertEqualsJSON(
      request, {utterance, rate: 3.0, voice: undefined, lang: undefined});
});

SYNC_TEST_F(
    'EnhancedNetworkTtsUnitTest', 'DecodeAudioDataAtSampleRate',
    async function() {
      const testAudioLength = 0.2135;
      let sampleRate = 4000;

      let audioBuffer = await EnhancedNetworkTts.decodeAudioDataAtSampleRate(
          generateTestBufferData(), sampleRate);
      assertEquals(
          audioBuffer.length, Math.floor(testAudioLength * sampleRate));

      sampleRate = 6000;
      audioBuffer = await EnhancedNetworkTts.decodeAudioDataAtSampleRate(
          generateTestBufferData(), sampleRate);
      assertEquals(
          audioBuffer.length, Math.floor(testAudioLength * sampleRate));

      sampleRate = 10000;
      audioBuffer = await EnhancedNetworkTts.decodeAudioDataAtSampleRate(
          generateTestBufferData(), sampleRate);
      assertEquals(
          audioBuffer.length, Math.floor(testAudioLength * sampleRate));
    });

SYNC_TEST_F('EnhancedNetworkTtsUnitTest', 'SubarrayFrom', async function() {
  const sampleArray = new Float32Array([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
  let subarray = EnhancedNetworkTts.subarrayFrom(
      sampleArray, 0 /* startIdx */, 5 /* subarraySize */);
  assertArraysEquals(subarray, new Float32Array([1, 2, 3, 4, 5]));

  subarray = EnhancedNetworkTts.subarrayFrom(
      sampleArray, 5 /* startIdx */, 5 /* subarraySize */);
  assertArraysEquals(subarray, new Float32Array([6, 7, 8, 9, 10]));

  subarray = EnhancedNetworkTts.subarrayFrom(
      sampleArray, 5 /* startIdx */, 10 /* subarraySize */);
  assertArraysEquals(
      subarray, new Float32Array([6, 7, 8, 9, 10, 0, 0, 0, 0, 0]));

  subarray = EnhancedNetworkTts.subarrayFrom(
      sampleArray, 11 /* startIdx */, 10 /* subarraySize */);
  assertArraysEquals(
      subarray, new Float32Array([0, 0, 0, 0, 0, 0, 0, 0, 0, 0]));
});

SYNC_TEST_F(
    'EnhancedNetworkTtsUnitTest', 'SendAudioDataInBuffers', async function() {
      // Prepare the decodedAudioData for testing. The data corresponds to a
      // 0.2135s playback.
      const testAudioLength = 0.2135;
      const sampleRate = 10000;
      const decodedAudioDataLength = testAudioLength * sampleRate;  // 2135
      const decodedAudioData =
          await EnhancedNetworkTts.decodeAudioDataAtSampleRate(
              generateTestBufferData(), sampleRate);
      assertEquals(decodedAudioData.length, decodedAudioDataLength);

      const timeInfo = generateTestTimeInfoData();
      // The |decodedAudioData| will be sent through 6 buffers. The first five
      // buffers has a size of 200, and the last one only has a size of 135.
      const bufferSize = 400;
      // Each buffers corresponds to 0.04s in the playback.

      const expectedBuffers = [
        // The first 0.04s is empty.
        {
          audioBuffer:
              EnhancedNetworkTts.subarrayFrom(decodedAudioData, 0, bufferSize),
          charIndex: undefined,
          isLastBuffer: false,
        },
        // 0.04s - 0.08s contains the start of the first word.
        {
          audioBuffer: EnhancedNetworkTts.subarrayFrom(
              decodedAudioData, bufferSize, bufferSize),
          charIndex: 0,
          isLastBuffer: false,
        },
        // 0.08s - 0.12s is a part of the first word.
        {
          audioBuffer: EnhancedNetworkTts.subarrayFrom(
              decodedAudioData, bufferSize * 2, bufferSize),
          charIndex: undefined,
          isLastBuffer: false,
        },
        // 0.12s - 0.16s contains the end of the first word and the start of the
        // second.
        {
          audioBuffer: EnhancedNetworkTts.subarrayFrom(
              decodedAudioData, bufferSize * 3, bufferSize),
          charIndex: 6,
          isLastBuffer: false,
        },
        // 0.16s - 0.2s is a part of the second word.
        {
          audioBuffer: EnhancedNetworkTts.subarrayFrom(
              decodedAudioData, bufferSize * 4, bufferSize),
          charIndex: undefined,
          isLastBuffer: false,
        },
        // The last buffer runs 0.0135s.
        {
          audioBuffer: EnhancedNetworkTts.subarrayFrom(
              decodedAudioData, bufferSize * 5, bufferSize),
          charIndex: undefined,
          isLastBuffer: true,
        },
      ];
      // Copy the expectedBuffers but modify |isLastBuffer|.
      const expectedBuffersWithoutLastBuffer =
          expectedBuffers.map(buffer => Object.assign({}, buffer));
      expectedBuffersWithoutLastBuffer[5].isLastBuffer = false;

      let mockSendTtsAudio = receivedBuffer => {
        const expectedBuffer = expectedBuffers.shift();
        assertEqualsJSON(expectedBuffer, receivedBuffer);
      };
      EnhancedNetworkTts.sendAudioDataInBuffers(
          decodedAudioData, sampleRate, bufferSize, timeInfo, mockSendTtsAudio,
          /* lastData= */ true);

      mockSendTtsAudio = receivedBuffer => {
        const expectedBuffer = expectedBuffersWithoutLastBuffer.shift();
        assertEqualsJSON(expectedBuffer, receivedBuffer);
      };
      // Will not signal the last buffer if |lastData| is false.
      EnhancedNetworkTts.sendAudioDataInBuffers(
          decodedAudioData, sampleRate, bufferSize, timeInfo, mockSendTtsAudio,
          /* lastData= */ false);
    });

/**
 * Generates audio data for testing. The audio data contains a clip of silent
 * audio originally in ogg format. The audio lasts 0.2135 seconds.
 * @return {!Array<number>}
 */
function generateTestBufferData() {
  const testData = [
    79,  103, 103, 83,  0,   2,   0,   0,   0,   0,   0,   0,   0,   0,   138,
    1,   148, 99,  0,   0,   0,   0,   19,  72,  225, 168, 1,   19,  79,  112,
    117, 115, 72,  101, 97,  100, 1,   1,   56,  1,   192, 93,  0,   0,   0,
    0,   0,   79,  103, 103, 83,  0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   138, 1,   148, 99,  1,   0,   0,   0,   225, 155, 98,  95,  1,   40,
    79,  112, 117, 115, 84,  97,  103, 115, 6,   0,   0,   0,   71,  111, 111,
    103, 108, 101, 1,   0,   0,   0,   14,  0,   0,   0,   101, 110, 99,  111,
    100, 101, 114, 61,  71,  111, 111, 103, 108, 101, 79,  103, 103, 83,  0,
    4,   208, 41,  0,   0,   0,   0,   0,   0,   138, 1,   148, 99,  2,   0,
    0,   0,   89,  72,  23,  6,   11,  14,  15,  16,  16,  16,  15,  15,  16,
    26,  20,  22,  104, 11,  228, 193, 34,  35,  97,  249, 140, 57,  108, 66,
    108, 208, 104, 7,   201, 121, 197, 18,  247, 188, 172, 131, 121, 219, 241,
    240, 200, 104, 7,   201, 121, 197, 19,  42,  199, 60,  74,  211, 193, 207,
    154, 151, 104, 104, 7,   201, 114, 39,  225, 62,  83,  8,   232, 252, 15,
    209, 23,  151, 104, 104, 7,   201, 114, 39,  225, 62,  83,  8,   232, 252,
    15,  209, 23,  151, 104, 104, 7,   201, 121, 197, 18,  247, 188, 172, 131,
    121, 250, 81,  240, 200, 104, 7,   201, 121, 197, 18,  247, 188, 172, 131,
    121, 250, 81,  240, 200, 104, 7,   201, 114, 39,  225, 62,  83,  8,   232,
    252, 15,  209, 23,  151, 104, 104, 0,   48,  103, 26,  34,  194, 86,  230,
    218, 164, 141, 181, 7,   252, 252, 185, 183, 181, 96,  71,  167, 143, 221,
    7,   142, 104, 12,  209, 169, 168, 188, 105, 184, 244, 111, 145, 71,  40,
    74,  40,  33,  234, 158, 16,  44,  104, 9,   73,  154, 65,  180, 184, 46,
    212, 58,  33,  41,  158, 252, 16,  100, 140, 106, 65,  21,  168, 221,
  ];
  return new Uint8Array(testData).buffer;
}

/**
 * Generates timestamp data for two words, "Hello world". The first 0.05s in the
 * playback is empty. "Hello" is spoken during 0.05 - 0.15s, "world" is spoken
 * during 0.15 - 0.21s. The last 0.0035s is empty, assuming the audio lasts
 * 0.2135 seconds.
 * @return {!Array<!ash.enhancedNetworkTts.mojom.TimingInfo>}
 */
function generateTestTimeInfoData() {
  return [
    // The first 0.05s in the playback is empty.
    {
      'text': 'Hello',
      'textOffset': 0,        // The word offset to the start of the utterance.
      'timeOffset': '0.05s',  // Start time at the entire playback.
      'duration': '0.1s',     // The word lasts 0.1s.
    },
    {
      'text': 'world',
      'textOffset': 6,        // The word offset to the start of the utterance.
      'timeOffset': '0.15s',  // Start time at the entire playback.
      'duration': '0.06s',    // The word lasts 0.6s.
    },                        // The last 0.0035s in the playback is empty.
  ];
}
