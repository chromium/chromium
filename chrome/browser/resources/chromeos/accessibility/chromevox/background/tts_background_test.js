// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_e2e_test_base.js']);

// E2E tests for PrimaryTts.

/** Test fixture. */
ChromeVoxBackgroundTtsTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    globalThis.tts = new PrimaryTts();
  }

  expectUtteranceQueueIsLike(expectedObjects) {
    const queue = tts.getUtteranceQueueForTest();
    assertEquals(expectedObjects.length, queue.length);
    for (let i = 0; i < expectedObjects.length; i++) {
      for (const key in expectedObjects[i]) {
        assertEquals(expectedObjects[i][key], queue[i][key]);
      }
    }
  }
};

// TODO(b/364477797): Failing on MSAN.
AX_TEST_F('ChromeVoxBackgroundTtsTest', 'MAYBE_Preprocess', function() {
  const preprocess = tts.preprocess.bind(tts);

  // Punctuation.
  assertEquals('dot', preprocess('.'));
  assertEquals('x.', preprocess('x.'));
  assertEquals('.x', preprocess('.x'));
  assertEquals('space', preprocess(' '));
  assertEquals('', preprocess('  '));
  assertEquals('A', preprocess('a'));
  assertEquals('A', preprocess('A'));
  assertEquals('a.', preprocess('a.'));
  assertEquals('.a', preprocess('.a'));
  assertEquals('Ａ', preprocess('ａ'));
  assertEquals('Б', preprocess('б'));

  // Only summarize punctuation if there are three or more occurrences without
  // a space in between.
  assertEquals('10 equal signs', preprocess('=========='));
  assertEquals('bullet bullet bullet', preprocess('\u2022 \u2022\u2022'));
  assertEquals('3 bullets', preprocess('\u2022\u2022\u2022'));
  assertEquals('C plus plus', preprocess('C++'));
  assertEquals('C 3 plus signs', preprocess('C+++'));
  // There are some punctuation marks that we do not verbalize because they
  // result in an overly verbose experience (periods, commas, dashes, etc.).
  // This set of punctuation marks is defined in the |some_punctuation| regular
  // expression in PrimaryTts.
  assertEquals('C--', preprocess('C--'));
  assertEquals('Hello world.', preprocess('Hello world.'));

  assertEquals('new line', preprocess('\n'));
  assertEquals('return', preprocess('\r'));

  // Assert that ChromeVox reads the pound sterling (£) sign.
  assertEquals('pound sterling', preprocess('£'));
  assertEquals(
      'This is the pound sterling symbol and this is the pound symbol.',
      preprocess('This is the £ symbol and this is the # symbol.'));
  // Repetitions for the pound sterling shouldn't be summarized.
  assertEquals(
      'pound sterling pound sterling pound sterling', preprocess('£££'));

  // Negative currency values.
  assertEquals('-1 dollar', preprocess('-$1'));
  assertEquals('-1.12 dollar', preprocess('-$1.12'));
  assertEquals('-1,123.58 dollar', preprocess('-$1,123.58'));
  assertEquals('-1,123,581.3 dollar', preprocess('-$1,123,581.3'));
  assertEquals('-1,123.58 pound sterling', preprocess('-£1,123.58'));

  // Bullets
  assertEquals('bullet', preprocess('\u2022'));
  assertEquals('3 bullets', preprocess('\u2022\u2022\u2022'));
  assertEquals('white bullet', preprocess('\u25e6'));
  assertEquals('3 white bullets', preprocess('\u25e6\u25e6\u25e6'));
  assertEquals('square bullet', preprocess('\u25a0'));
  assertEquals('3 square bullets', preprocess('\u25a0\u25a0\u25a0'));

  assertEquals('space', preprocess('\u00a0'));
}, `
#if defined(MEMORY_SANITIZER)
#define MAYBE_Preprocess DISABLED_Preprocess
#else
#define MAYBE_Preprocess Preprocess
#endif
`);

TEST_F('ChromeVoxBackgroundTtsTest', 'UpdateVoice', function() {
  const voices = [
    {lang: 'zh-CN', voiceName: 'Chinese'},
    {lang: 'zh-TW', voiceName: 'Chinese (Taiwan)'},
    {lang: 'es', voiceName: 'Spanish'},
    {lang: 'en-US', voiceName: 'U.S. English'},
  ];

  chrome.tts.getVoices = function(callback) {
    callback(voices);
  };

  // Asks this test to process the next task immediately.
  const flushNextTask = () => {
    const task = tasks.shift();
    if (!task) {
      return;
    }

    if (task.setup) {
      task.setup();
    }
    tts.updateVoice(task.testVoice, this.newCallback(function(actualVoice) {
      assertEquals(task.expectedVoice, actualVoice);
      flushNextTask();
    }));
  };

  assertTrue(!tts.currentVoice);

  const tasks = [
    {testVoice: '', expectedVoice: constants.SYSTEM_VOICE},

    {
      setup() {
        voices[3].lang = 'en';
      },
      testVoice: 'U.S. English',
      expectedVoice: 'U.S. English',
    },

    {
      setup() {
        voices[3].lang = 'fr-FR';
        voices[3].voiceName = 'French';
      },
      testVoice: '',
      expectedVoice: constants.SYSTEM_VOICE,
    },

    {testVoice: 'French', expectedVoice: 'French'},

    {testVoice: 'NotFound', expectedVoice: constants.SYSTEM_VOICE},
  ];

  flushNextTask();
});

// This test only works if Google tts is installed. Run it locally.
TEST_F(
    'ChromeVoxBackgroundTtsTest', 'DISABLED_EmptyStringCallsCallbacks',
    function() {
      let startCalls = 0;
      let endCalls = 0;
      assertCallsCallbacks = (text, speakCalls) => {
        tts.speak(text, QueueMode.QUEUE, new TtsSpeechProperties({
                    startCallback() {
                      ++startCalls;
                    },
                    endCallback: this.newCallback(function() {
                      ++endCalls;
                      assertEquals(speakCalls, endCalls);
                      assertEquals(endCalls, startCalls);
                    }),
                  }));
      };

      assertCallsCallbacks('', 1);
      assertCallsCallbacks('  ', 2);
      assertCallsCallbacks(' \u00a0 ', 3);
    });

AX_TEST_F(
    'ChromeVoxBackgroundTtsTest', 'CapitalizeSingleLettersAfterNumbers',
    function() {
      const preprocess = tts.preprocess.bind(tts);

      // Capitalize single letters if they appear directly after a number.
      assertEquals(
          'Click to join the 5G network',
          preprocess('Click to join the 5g network'));
      assertEquals(
          'I ran a 100M sprint in 10 S',
          preprocess('I ran a 100m sprint in 10 s'));
      // Do not capitalize the letter "a".
      assertEquals(
          'Please do the shopping at 3 a thing came up at work',
          preprocess('Please do the shopping at 3 a thing came up at work'));
    });

AX_TEST_F('ChromeVoxBackgroundTtsTest', 'AnnounceCapitalLetters', function() {
  const preprocess = tts.preprocess.bind(tts);

  assertEquals('A', preprocess('A'));

  // Only announce capital for solo capital letters.
  SettingsManager.set('capitalStrategy', 'announceCapitals');
  assertEquals('Cap A', preprocess('A'));
  assertEquals('Cap Z', preprocess('Z'));
  assertEquals('Cap Ａ', preprocess('Ａ'));
  assertEquals('Cap Б', preprocess('Б'));

  // Do not announce capital for the following inputs.
  assertEquals('BB', preprocess('BB'));
  assertEquals('A.', preprocess('A.'));
});

AX_TEST_F('ChromeVoxBackgroundTtsTest', 'PunctuationMode', function() {
  const PUNCTUATION_ECHO_NONE = '0';
  const PUNCTUATION_ECHO_SOME = '1';
  const PUNCTUATION_ECHO_ALL = '2';

  const updatePunctuationEcho = tts.updatePunctuationEcho.bind(tts);
  let lastSpokenTextString = '';
  tts.speakUsingQueue_ = function(utterance, _) {
    lastSpokenTextString = utterance.textString;
  };

  // No punctuation.
  updatePunctuationEcho(PUNCTUATION_ECHO_NONE);

  tts.speak(`"That's all, folks!"`);
  assertEquals(`That's all, folks!`, lastSpokenTextString);

  tts.speak('"$1,234.56 (plus tax) for 78% of your #2 pencils?", they mused');
  assertEquals(
      '1,234.56 plus tax for 78% of your 2 pencils? , they mused',
      lastSpokenTextString);

  // Some punctuation.
  updatePunctuationEcho(PUNCTUATION_ECHO_SOME);

  tts.speak(`"That's all, folks!"`);
  assertEquals(`quote That's all, folks! quote`, lastSpokenTextString);

  tts.speak('"$1,234.56 (plus tax) for 78% of your #2 pencils?", they mused');
  assertEquals(
      'quote dollar 1,234.56 (plus tax) for 78 percent of your pound 2 ' +
          'pencils? quote , they mused',
      lastSpokenTextString);

  // All punctuation.
  updatePunctuationEcho(PUNCTUATION_ECHO_ALL);

  tts.speak(`"That's all, folks!"`);
  assertEquals(
      `quote That apostrophe' s all comma folks exclamation! quote`,
      lastSpokenTextString);

  tts.speak('"$1,234.56 (plus tax) for 78% of your #2 pencils?", they mused');
  assertEquals(
      'quote dollar 1 comma 234 dot 56 open paren plus tax close paren for ' +
          '78 percent of your pound 2 pencils question mark? quote comma ' +
          'they mused',
      lastSpokenTextString);
});

AX_TEST_F('ChromeVoxBackgroundTtsTest', 'NumberReadingStyle', function() {
  let lastSpokenTextString = '';
  tts.speakUsingQueue_ = function(utterance, _) {
    lastSpokenTextString = utterance.textString;
  };

  // Check the default preference.
  assertEquals('asWords', SettingsManager.get('numberReadingStyle'));

  tts.speak('100');
  assertEquals('100', lastSpokenTextString);

  tts.speak('An unanswered call lasts for 30 seconds.');
  assertEquals(
      'An unanswered call lasts for 30 seconds.', lastSpokenTextString);

  tts.speak('１００');
  assertEquals('１００', lastSpokenTextString);

  tts.speak('An unanswered call lasts for ３０ seconds.');
  assertEquals(
      'An unanswered call lasts for ３０ seconds.', lastSpokenTextString);

  SettingsManager.set('numberReadingStyle', 'asDigits');
  tts.speak('100');
  assertEquals('1 0 0', lastSpokenTextString);

  tts.speak('An unanswered call lasts for 30 seconds.');
  assertEquals(
      'An unanswered call lasts for 3 0 seconds.', lastSpokenTextString);

  tts.speak('１００');
  assertEquals('１ ０ ０', lastSpokenTextString);

  tts.speak('An unanswered call lasts for ３０ seconds.');
  assertEquals(
      'An unanswered call lasts for ３ ０ seconds.', lastSpokenTextString);
});

AX_TEST_F('ChromeVoxBackgroundTtsTest', 'SplitLongText', function() {
  const spokenTextStrings = [];
  tts.speakUsingQueue_ = function(utterance, _) {
    spokenTextStrings.push(utterance.textString);
  };

  // There are three new lines, but only the first chunk exceeds the max
  // character count.
  const text = 'a'.repeat(constants.OBJECT_MAX_CHARCOUNT) + '\n' +
      'a'.repeat(10) + '\n' +
      'a'.repeat(10);
  tts.speak(text);
  assertEquals(2, spokenTextStrings.length);
});

AX_TEST_F('ChromeVoxBackgroundTtsTest', 'SplitUntilSmall', function() {
  const split = PrimaryTts.splitUntilSmall;

  // A single delimiter.
  constants.OBJECT_MAX_CHARCOUNT = 3;
  assertEqualsJSON(['12', '345', '789'], split('12345a789', 'a'));

  constants.OBJECT_MAX_CHARCOUNT = 4;
  assertEqualsJSON(['12', '345', '789'], split('12345a789', 'a'));

  constants.OBJECT_MAX_CHARCOUNT = 7;
  assertEqualsJSON(['12345', '789'], split('12345a789', 'a'));

  constants.OBJECT_MAX_CHARCOUNT = 10;
  assertEqualsJSON(['12345a789'], split('12345a789', 'a'));

  // Multiple delimiters.
  constants.OBJECT_MAX_CHARCOUNT = 3;
  assertEqualsJSON(['12', '34', '57', '89'], split('1234b57a89', 'ab'));

  constants.OBJECT_MAX_CHARCOUNT = 4;
  assertEqualsJSON(['1234', '57', '89'], split('1234b57a89', 'ab'));

  constants.OBJECT_MAX_CHARCOUNT = 5;
  assertEqualsJSON(['12345', '789'], split('12345b789a', 'ab'));
  assertEqualsJSON(['12345', '789a'], split('12345b789a', 'ba'));

  // No delimiters.
  constants.OBJECT_MAX_CHARCOUNT = 3;
  assertEqualsJSON(['12', '34', '57', '89'], split('12345789', ''));

  constants.OBJECT_MAX_CHARCOUNT = 4;
  assertEqualsJSON(['1234', '5789'], split('12345789', ''));

  // Some corner cases.
  assertEqualsJSON([], split('', ''));
  assertEqualsJSON(['a'], split('a', ''));
  assertEqualsJSON(['a'], split('a', 'a'));
  assertEqualsJSON(['a'], split('a', 'b'));
});

AX_TEST_F('ChromeVoxBackgroundTtsTest', 'Phonetics', function() {
  let spokenStrings = [];
  tts.speakUsingQueue_ = (utterance, ...rest) => {
    spokenStrings.push(utterance.textString);
  };

  const checkFor = str => assertTrue(
      spokenStrings.includes(str), 'spoken strings should include ' + str);

  // English.
  tts.speak(
      't', QueueMode.QUEUE,
      new TtsSpeechProperties({lang: 'en-us', phoneticCharacters: true}));
  checkFor('T');
  checkFor('tango');
  tts.speak(
      'a', QueueMode.QUEUE,
      new TtsSpeechProperties({lang: 'en-us', phoneticCharacters: true}));
  checkFor('A');
  checkFor('alpha');
  spokenStrings = [];

  // German.
  tts.speak(
      't', QueueMode.QUEUE,
      new TtsSpeechProperties({lang: 'de', phoneticCharacters: true}));
  checkFor('T');
  checkFor('Theodor');
  tts.speak(
      'a', QueueMode.QUEUE,
      new TtsSpeechProperties({lang: 'de', phoneticCharacters: true}));
  checkFor('A');
  checkFor('Anton');
  spokenStrings = [];

  // Japanese.
  tts.speak(
      't', QueueMode.QUEUE,
      new TtsSpeechProperties({lang: 'ja', phoneticCharacters: true}));
  checkFor('T');
  checkFor('ティー タイム');

  tts.speak(
      'a', QueueMode.QUEUE,
      new TtsSpeechProperties({lang: 'ja', phoneticCharacters: true}));
  checkFor('A');
  checkFor('エイ アニマル');

  tts.speak(
      'A', QueueMode.QUEUE,
      new TtsSpeechProperties({lang: 'ja', phoneticCharacters: true}));
  checkFor('A');
  checkFor('エイ アニマル');

  tts.speak(
      '人', QueueMode.QUEUE,
      new TtsSpeechProperties({lang: 'ja', phoneticCharacters: true}));
  checkFor('人');
  checkFor('ヒト，ニンゲン ノ ニン');
  spokenStrings = [];

  // Error handling.
  tts.speak(
      't', QueueMode.QUEUE,
      new TtsSpeechProperties({lang: 'qwerty', phoneticCharacters: true}));
  checkFor('T');
  assertEquals(1, spokenStrings.length);
});

AX_TEST_F('ChromeVoxBackgroundTtsTest', 'PitchChanges', function() {
  const preprocess = tts.preprocess.bind(tts);
  const props = {relativePitch: -0.3};
  SettingsManager.set('usePitchChanges', true);
  preprocess('Hello world', props);
  assertTrue(props.hasOwnProperty('relativePitch'));
  SettingsManager.set('usePitchChanges', false);
  preprocess('Hello world', props);
  assertFalse(props.hasOwnProperty('relativePitch'));
});

AX_TEST_F('ChromeVoxBackgroundTtsTest', 'InterjectUtterances', function() {
  // Fake out setTimeout for our purposes.
  let lastSetTimeoutCallback;
  setTimeout = (callback, delay) => {
    lastSetTimeoutCallback = callback;
  };

  // Mock out to not trigger any events.
  chrome.tts.speak = () => {};

  // Flush and queue a few utterances to build the speech queue.
  tts.speak('Hi', QueueMode.FLUSH);
  tts.speak('there.', QueueMode.QUEUE);
  tts.speak('How are you?', QueueMode.QUEUE);

  // Verify the contents of the speech queue at this point.
  this.expectUtteranceQueueIsLike([
    {textString: 'Hi', queueMode: QueueMode.FLUSH},
    {textString: 'there.', queueMode: QueueMode.QUEUE},
    {textString: 'How are you?', queueMode: QueueMode.QUEUE},
  ]);

  // Interject a single utterance now.
  tts.speak('Sorry; busy!', QueueMode.INTERJECT);
  this.expectUtteranceQueueIsLike(
      [{textString: 'Sorry; busy!', queueMode: QueueMode.INTERJECT}]);

  // The above call should have resulted in a setTimeout; call it.
  assertTrue(Boolean(lastSetTimeoutCallback));
  lastSetTimeoutCallback();
  lastSetTimeoutCallback = undefined;

  // The previous utterances should now be restored.
  this.expectUtteranceQueueIsLike([
    {textString: 'Sorry; busy!', queueMode: QueueMode.INTERJECT},
    {textString: 'Hi', queueMode: QueueMode.FLUSH},
    {textString: 'there.', queueMode: QueueMode.QUEUE},
    {textString: 'How are you?', queueMode: QueueMode.QUEUE},
  ]);

  // Try interjecting again. Notice it interrupts the previous interjection.
  tts.speak('Actually, not busy after all!', QueueMode.INTERJECT);
  this.expectUtteranceQueueIsLike([{
    textString: 'Actually, not busy after all!',
    queueMode: QueueMode.INTERJECT,
  }]);

  // Before the end of the current callstack, simulated by calling the callback
  // to setTimeout, we can keep calling speak. These are also interjections (see
  // below).
  tts.speak('I am good.', QueueMode.QUEUE);
  tts.speak('How about you?', QueueMode.QUEUE);
  this.expectUtteranceQueueIsLike([
    {
      textString: 'Actually, not busy after all!',
      queueMode: QueueMode.INTERJECT,
    },
    {textString: 'I am good.', queueMode: QueueMode.QUEUE},
    {textString: 'How about you?', queueMode: QueueMode.QUEUE},
  ]);

  // The above call should have resulted in a setTimeout; call it.
  assertTrue(Boolean(lastSetTimeoutCallback));
  lastSetTimeoutCallback();
  lastSetTimeoutCallback = undefined;

  // The previous utterances should now be restored.
  this.expectUtteranceQueueIsLike([
    {
      textString: 'Actually, not busy after all!',
      queueMode: QueueMode.INTERJECT,
    },
    {textString: 'I am good.', queueMode: QueueMode.INTERJECT},
    {textString: 'How about you?', queueMode: QueueMode.INTERJECT},
    {textString: 'Hi', queueMode: QueueMode.FLUSH},
    {textString: 'there.', queueMode: QueueMode.QUEUE},
    {textString: 'How are you?', queueMode: QueueMode.QUEUE},
  ]);

  // Interject again. Notice all previous interjections get cancelled again.
  // This is crucial to not leak utterances out of the chaining that some
  // modules like Output do.
  tts.speak('Sorry! Gotta go!', QueueMode.INTERJECT);
  this.expectUtteranceQueueIsLike(
      [{textString: 'Sorry! Gotta go!', queueMode: QueueMode.INTERJECT}]);
  assertTrue(Boolean(lastSetTimeoutCallback));
  lastSetTimeoutCallback();
  lastSetTimeoutCallback = undefined;

  // All other interjections except the last one are gone.
  this.expectUtteranceQueueIsLike([
    {textString: 'Sorry! Gotta go!', queueMode: QueueMode.INTERJECT},
    {textString: 'Hi', queueMode: QueueMode.FLUSH},
    {textString: 'there.', queueMode: QueueMode.QUEUE},
    {textString: 'How are you?', queueMode: QueueMode.QUEUE},
  ]);
});

AX_TEST_F('ChromeVoxBackgroundTtsTest', 'Mute', function() {
  // Fake out setTimeout for our purposes.
  let lastSetTimeoutCallback;
  setTimeout = (callback, delay) => {
    lastSetTimeoutCallback = callback;
  };

  // Mock this to ensure no events are triggered.
  chrome.tts.speak = () => {};
  // Push some text into the speech queue and verify state.
  tts.speak('Hello', QueueMode.FLUSH);
  tts.speak('world.', QueueMode.QUEUE);
  this.expectUtteranceQueueIsLike([
    {textString: 'Hello', queueMode: QueueMode.FLUSH},
    {textString: 'world.', queueMode: QueueMode.QUEUE},
  ]);

  // Toggle speech off.
  tts.toggleSpeechOnOrOff();

  // The above call should have resulted in a setTimeout; call it.
  assertTrue(Boolean(lastSetTimeoutCallback));
  lastSetTimeoutCallback();
  lastSetTimeoutCallback = undefined;

  // Make assertions.
  assertEquals(null, tts.currentUtterance_);
  this.expectUtteranceQueueIsLike([]);
});

TEST_F('ChromeVoxBackgroundTtsTest', 'ResetTtsSettingsClearsVoice', function() {
  this.newCallback(async () => {
    ChromeVox.tts.ttsEngines_[0].currentVoice = '';
    CommandHandlerInterface.instance.onCommand('resetTextToSpeechSettings');
    await new Promise(r => {
      ChromeVox.tts.speak = textString => {
        if (textString === 'Reset text to speech settings to default values') {
          r();
        }
      };
    });
    assertEquals(
        constants.SYSTEM_VOICE, ChromeVox.tts.ttsEngines_[0].currentVoice);
  })();
});
