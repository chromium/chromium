// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE([
  '../../testing/chromevox_e2e_test_base.js',
  '../../../common/testing/assert_additions.js',
]);

/**
 * Test fixture for intent handler tests.
 * These tests are written to be as "unit" test like as possible e.g. mocking
 * out classes not under test but it runs under a full extension test
 * environment to get things like extension api literals.
 */
ChromeVoxIntentHandlerTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    globalThis.Dir = constants.Dir;
    globalThis.IntentTextBoundaryType =
        chrome.automation.IntentTextBoundaryType;
  }
};

AX_TEST_F('ChromeVoxIntentHandlerTest', 'MoveByCharacter', function() {
  let calls = [];
  const fakeLine = class {
    constructor(startOffset) {
      this.startOffset_ = startOffset;
    }

    createCharRange() {
      calls.push(['createCharRange']);
      return {};
    }

    get startOffset() {
      return this.startOffset_;
    }

    get text() {
      return 'hello';
    }
  };

  Output.prototype.withRichSpeech = function(...args) {
    calls.push(['withRichSpeechAndBraille', ...args]);
    return this;
  };

  Output.prototype.go = function() {
    calls.push(['go']);
  };

  const intent = {textBoundary: IntentTextBoundaryType.CHARACTER};
  const move = IntentHandler.onMoveSelection.bind(null, intent);

  move(new fakeLine(0));
  assertEquals(3, calls.length);
  assertArraysEquals(['createCharRange'], calls[0]);
  assertArraysEquals(
      ['withRichSpeechAndBraille', {}, null, OutputCustomEvent.NAVIGATE],
      calls[1]);
  assertArraysEquals(['go'], calls[2]);

  calls = [];
  move(new fakeLine(1), new fakeLine(0));
  assertEquals(4, calls.length);
  assertArraysEquals(['createCharRange'], calls[0]);
  assertArraysEquals(['createCharRange'], calls[1]);
  assertArraysEquals(
      ['withRichSpeechAndBraille', {}, {}, OutputCustomEvent.NAVIGATE],
      calls[2]);
  assertArraysEquals(['go'], calls[3]);
});

AX_TEST_F('ChromeVoxIntentHandlerTest', 'MoveByWord', function() {
  let calls = [];
  const fakeLine = new (class {
    createWordRange(...args) {
      calls.push(['createWordRange', ...args]);
      return {};
    }
  })();

  Output.prototype.withSpeech = function(...args) {
    calls.push(['withSpeech', ...args]);
    return this;
  };

  Output.prototype.go = function() {
    calls.push(['go']);
  };

  let intent = {textBoundary: IntentTextBoundaryType.WORD_END};
  IntentHandler.onMoveSelection(intent, fakeLine);
  assertEquals(3, calls.length);
  assertArraysEquals(['createWordRange', true], calls[0]);
  assertArraysEquals(
      ['withSpeech', {}, null, OutputCustomEvent.NAVIGATE], calls[1]);
  assertArraysEquals(['go'], calls[2]);

  calls = [];
  intent = {textBoundary: IntentTextBoundaryType.WORD_START};
  IntentHandler.onMoveSelection(intent, fakeLine);
  assertEquals(3, calls.length);
  assertArraysEquals(['createWordRange', false], calls[0]);
  assertArraysEquals(
      ['withSpeech', {}, null, OutputCustomEvent.NAVIGATE], calls[1]);
  assertArraysEquals(['go'], calls[2]);

  calls = [];
  intent = {textBoundary: IntentTextBoundaryType.WORD_START_OR_END};
  IntentHandler.onMoveSelection(intent, fakeLine);
  assertEquals(0, calls.length);
});

AX_TEST_F('ChromeVoxIntentHandlerTest', 'MoveByLine', function() {
  const fakeLine = new (class {
    constructor() {
      this.speakLineCount = 0;
    }

    speakLine() {
      this.speakLineCount++;
    }

    get start() {
      return new (class {
        move() {}
      })();
    }
  })();

  Output.prototype.withSpeech = function() {
    return this;
  };
  Output.prototype.go = function() {};

  let intent = {textBoundary: IntentTextBoundaryType.LINE_END};
  IntentHandler.onMoveSelection(intent, fakeLine);
  assertEquals(1, fakeLine.speakLineCount);

  intent = {textBoundary: IntentTextBoundaryType.LINE_START};
  IntentHandler.onMoveSelection(intent, fakeLine);
  assertEquals(2, fakeLine.speakLineCount);

  intent = {textBoundary: IntentTextBoundaryType.LINE_START_OR_END};
  IntentHandler.onMoveSelection(intent, fakeLine);
  assertEquals(3, fakeLine.speakLineCount);
});
