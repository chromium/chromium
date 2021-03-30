// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE([
  '../../testing/chromevox_next_e2e_test_base.js',
  '../../../common/testing/assert_additions.js',
]);

/**
 * Test fixture for intent handler tests.
 * These tests are written to be as "unit" test like as possible e.g. mocking
 * out classes not under test but it runs under a full extension test
 * environment to get things like extension api literals.
 */
ChromeVoxIntentHandlerTest = class extends ChromeVoxNextE2ETest {
  /** @override */
  setUp() {
    window.Dir = constants.Dir;
    window.IntentTextBoundaryType = chrome.automation.IntentTextBoundaryType;
    window.Movement = cursors.Movement;
    window.Unit = cursors.Unit;
  }
};

SYNC_TEST_F('ChromeVoxIntentHandlerTest', 'MoveByCharacter', function() {
  let lastSpoken;
  ChromeVox.tts.speak = (text) => lastSpoken = text;

  const intent = {textBoundary: IntentTextBoundaryType.CHARACTER};
  const move = IntentHandler.onMoveSelection.bind(null, intent);

  move({text: 'hello', startOffset: 0});
  assertEquals('h', lastSpoken);
  move({text: 'hello', startOffset: 1});
  assertEquals('e', lastSpoken);
  move({text: 'hello', startOffset: 2});
  assertEquals('l', lastSpoken);
  move({text: 'hello', startOffset: 3});
  assertEquals('l', lastSpoken);
  move({text: 'hello', startOffset: 4});
  assertEquals('o', lastSpoken);
  move({text: 'hello', startOffset: 5});
  assertEquals('\n', lastSpoken);
});

SYNC_TEST_F('ChromeVoxIntentHandlerTest', 'MoveByWord', function() {
  let calls = [];
  const fakeLine = new class {
    constructor() {}

    get startCursor() {
      return new class {
        move(...args) {
          calls.push(['move', ...args]);
        }
      };
    }
  };

  Output.prototype.withSpeech = function(...args) {
    calls.push(['withSpeech', ...args]);
    return this;
  };

  Output.prototype.go = function() {
    calls.push(['go']);
  };

  let intent = {textBoundary: IntentTextBoundaryType.WORD_END};
  IntentHandler.onMoveSelection(intent, fakeLine);
  assertEquals(4, calls.length);
  assertArraysEquals(
      ['move', Unit.WORD, Movement.DIRECTIONAL, Dir.BACKWARD], calls[0]);
  assertArraysEquals(
      ['move', Unit.WORD, Movement.BOUND, Dir.FORWARD], calls[1]);
  assertArraysEquals(
      ['withSpeech', {}, null, Output.EventType.NAVIGATE], calls[2]);
  assertArraysEquals(['go'], calls[3]);

  calls = [];
  intent = {textBoundary: IntentTextBoundaryType.WORD_START};
  IntentHandler.onMoveSelection(intent, fakeLine);
  assertEquals(4, calls.length);
  assertArraysEquals(
      ['move', Unit.WORD, Movement.BOUND, Dir.BACKWARD], calls[0]);
  assertArraysEquals(
      ['move', Unit.WORD, Movement.BOUND, Dir.FORWARD], calls[1]);
  assertArraysEquals(
      ['withSpeech', {}, null, Output.EventType.NAVIGATE], calls[2]);
  assertArraysEquals(['go'], calls[3]);

  calls = [];
  intent = {textBoundary: IntentTextBoundaryType.WORD_START_OR_END};
  IntentHandler.onMoveSelection(intent, fakeLine);
  assertEquals(0, calls.length);
});

SYNC_TEST_F('ChromeVoxIntentHandlerTest', 'MoveByLine', function() {
  const fakeLine = new class {
    constructor() {
      this.speakLineCount = 0;
    }

    speakLine() {
      this.speakLineCount++;
    }

    get startCursor() {
      return new class {
        move() {}
      };
    }
  };

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

  intent = {textBoundary: IntentTextBoundaryType.WORD_START};
  IntentHandler.onMoveSelection(intent, fakeLine);
  assertEquals(3, fakeLine.speakLineCount);
});
