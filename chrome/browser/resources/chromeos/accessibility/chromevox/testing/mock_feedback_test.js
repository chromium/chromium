// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../common/testing/accessibility_test_base.js']);

function speak(text, opt_properties) {
  ChromeVox.tts.speak(text, 0, opt_properties);
}

function braille(text) {
  const navBraille = NavBraille.fromText(text);
  ChromeVox.braille.write(navBraille);
  return navBraille;
}

function earcon(earconName) {
  ChromeVox.earcons.playEarcon(Earcon[earconName]);
}

/**
 * Test fixture.
 */
MockFeedbackUnitTest = class extends AccessibilityTestBase {
  constructor() {
    super();
    this.expectedCalls = [];
  }

  async setUpDeferred() {
    await super.setUpDeferred();

    // Alphabetical based on file path.
    await importModule(
        'BrailleInterface',
        '/chromevox/background/braille/braille_interface.js');
    await importModule('ChromeVox', '/chromevox/background/chromevox.js');
    await importModule(
        ['AbstractEarcons', 'Earcon'], '/chromevox/common/abstract_earcons.js');
    await importModule(
        'NavBraille', '/chromevox/common/braille/nav_braille.js');
    await importModule('Spannable', '/chromevox/common/spannable.js');
    await importModule('QueueMode', '/chromevox/common/tts_types.js');
  }

  /** @override */
  setUp() {
    super.setUp();
    window.ChromeVox = window.ChromeVox || {};
  }
};

MockFeedbackUnitTest.prototype.extraLibraries = [
  '../../common/testing/assert_additions.js',
  '../testing/fake_dom.js',  // Must come before other files
  'mock_feedback.js',
];

TEST_F('MockFeedbackUnitTest', 'speechAndCallbacks', function() {
  let afterThirdStringCalled = false;
  let spruiousStringEndCallbackCalled = false;
  let finishCalled = false;
  const mock = new MockFeedback(function() {
    assertFalse(finishCalled);
    finishCalled = true;

    assertTrue(afterThirdStringCalled);
    assertTrue(spruiousStringEndCallbackCalled);
  });
  mock.install();
  speak('First string');
  speak('Second string');
  mock.expectSpeech('First string', 'Second string')
      .expectSpeech('Third string')
      .call(function() {
        assertFalse(afterThirdStringCalled);
        afterThirdStringCalled = true;
        speak('Spurious string', {
          endCallback() {
            assertFalse(spruiousStringEndCallbackCalled);
            spruiousStringEndCallbackCalled = true;
          },
        });
        speak('Fourth string');
      })
      .expectSpeech('Fourth string')
      .replay();
  assertFalse(finishCalled);
  speak('Third string');
  assertTrue(finishCalled);
});

TEST_F('MockFeedbackUnitTest', 'startAndEndCallbacks', function() {
  let onlyStartCallbackCalled = false;
  let onlyEndCallbackCalled = false;
  let bothCallbacksStartCalled = false;
  let bothCallbacksEndCalled = false;
  const mock = new MockFeedback();
  mock.install();
  speak('No callbacks', {});
  speak('Only start callback', {
    startCallback() {
      assertFalse(onlyStartCallbackCalled);
      onlyStartCallbackCalled = true;
      assertFalse(onlyEndCallbackCalled);
    },
  });
  speak('Only end callback', {
    endCallback() {
      assertTrue(onlyStartCallbackCalled);
      assertFalse(onlyEndCallbackCalled);
      onlyEndCallbackCalled = true;
      assertFalse(bothCallbacksStartCalled);
    },
  });
  speak('Both callbacks', {
    startCallback() {
      assertTrue(onlyEndCallbackCalled);
      assertFalse(bothCallbacksStartCalled);
      bothCallbacksStartCalled = true;
      assertFalse(bothCallbacksEndCalled);
    },
    endCallback() {
      assertTrue(bothCallbacksStartCalled);
      assertFalse(bothCallbacksEndCalled);
      bothCallbacksEndCalled = true;
    },
  });
  mock.expectSpeech('Both callbacks');
  mock.replay();
  assertTrue(bothCallbacksEndCalled);
});

TEST_F('MockFeedbackUnitTest', 'SpeechAndBraille', function() {
  let secondCallbackCalled = false;
  let finishCalled = false;
  const mock = new MockFeedback(function() {
    finishCalled = true;
  });
  let firstExpectedNavBraille;
  mock.install();
  braille('Some braille');
  speak('Some speech');
  mock.call(function() {
        assertEquals(null, mock.lastMatchedBraille);
        firstExpectedNavBraille = braille('First expected braille');
        speak('First expected speech');
        braille('Some other braille');
      })
      .expectSpeech('First expected speech')
      .expectBraille('First expected braille')
      .call(function() {
        secondCallbackCalled = true;
        assertEquals(firstExpectedNavBraille, mock.lastMatchedBraille);
      })
      .replay();
  assertTrue(secondCallbackCalled);
  assertTrue(finishCalled);
});

TEST_F('MockFeedbackUnitTest', 'expectWithRegex', function() {
  let done = false;
  const mock = new MockFeedback();
  mock.install();
  mock.call(function() {
        braille('Item 1 of 14');
      })
      .expectBraille(/Item \d+ of \d+/)
      .call(function() {
        done = true;
      })
      .replay();
  assertTrue(done);
});

TEST_F('MockFeedbackUnitTest', 'expectAfterReplayThrows', function() {
  const mock = new MockFeedback();
  mock.replay();
  assertException('', function() {
    mock.expectSpeech('hello');
  }, 'AssertionError');
});

TEST_F('MockFeedbackUnitTest', 'NoMatchDoesNotFinish', function() {
  let firstCallbackCalled = false;
  const mock = new MockFeedback(function() {
    throw Error('Should not be called');
  });
  mock.install();
  braille('Some string');
  mock.call(function() {
        braille('Some other string');
        firstCallbackCalled = true;
      })
      .expectBraille('Unmatched string')
      .call(function() {
        throw Error('Should not be called');
      })
      .replay();
  assertTrue(firstCallbackCalled);
});

TEST_F('MockFeedbackUnitTest', 'SpeechAndEarcons', function() {
  let finishCalled = false;
  const mock = new MockFeedback(function() {
    finishCalled = true;
  });
  mock.install();
  mock.call(function() {
        speak('MyButton', {
          startCallback() {
            earcon('BUTTON');
          },
        });
      })
      .expectSpeech('MyButton')
      .expectEarcon(Earcon.BUTTON)
      .call(function() {
        earcon('ALERT_MODAL');
        speak('MyTextField', {
          startCallback() {
            earcon('EDITABLE_TEXT');
          },
        });
      })
      .expectEarcon(Earcon.ALERT_MODAL)
      .expectSpeech('MyTextField')
      .expectEarcon(Earcon.EDITABLE_TEXT)
      .replay();
  assertTrue(finishCalled);
});

TEST_F('MockFeedbackUnitTest', 'SpeechWithLanguage', function() {
  let finishCalled = false;
  const mock = new MockFeedback(function() {
    finishCalled = true;
  });
  mock.install();

  mock.call(function() {
        speak('This is English', {lang: 'en'});
        speak('This is also English', {lang: 'en'});
      })
      .expectSpeechWithLocale('en', 'This is English', 'This is also English')
      .call(function() {
        speak('Expect French', {lang: 'fr'});
      })
      .expectSpeechWithLocale('fr', 'Expect French')
      .call(function() {
        speak('Expect Canadian French', {lang: 'fr-ca'});
      })
      .expectSpeechWithLocale('fr-ca', 'Expect Canadian French')
      .call(function() {
        speak('Expect empty language', {lang: ''});
      })
      .expectSpeechWithLocale('', 'Expect empty language')
      .call(function() {
        speak('Expect no language');
      })
      .expectSpeechWithLocale(undefined, 'Expect no language')
      .replay();
  assertTrue(finishCalled);
});
