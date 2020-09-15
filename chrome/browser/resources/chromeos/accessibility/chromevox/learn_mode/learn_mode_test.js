// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '../testing/chromevox_next_e2e_test_base.js',
]);

/**
 * Test fixture for ChromeVox Learn Mode page.
 */
ChromeVoxLearnModeTest = class extends ChromeVoxNextE2ETest {
  constructor() {
    super();
    window.doKeyDown = this.doKeyDown.bind(this);
    window.doKeyUp = this.doKeyUp.bind(this);
    window.doGesture = this.doGesture.bind(this);
    window.doBrailleKeyEvent = this.doBrailleKeyEvent.bind(this);
  }

  runOnLearnModePage(callback) {
    const mockFeedback = this.createMockFeedback();
    chrome.automation.getDesktop((desktop) => {
      desktop.addEventListener(
          chrome.automation.EventType.LOAD_COMPLETE, (evt) => {
            if (evt.target.docUrl.indexOf('learn_mode/kbexplorer.html') == -1 ||
                !evt.target.docLoaded) {
              return;
            }

            mockFeedback.expectSpeech(/Press a qwerty key/);
            callback(mockFeedback, evt);
          });
      CommandHandler.onCommand('showKbExplorerPage');
    });
  }

  getLearnModeWindow() {
    let learnModeWindow = null;
    while (!learnModeWindow) {
      learnModeWindow = chrome.extension.getViews().find(function(view) {
        return view.location.href.indexOf(
                   'chromevox/learn_mode/kbexplorer.html') > 0;
      });
    }
    return learnModeWindow;
  }

  makeMockKeyEvent(params) {
    // Fake out these functions.
    params.preventDefault = () => {};
    params.stopPropagation = () => {};

    // Set defaults if not defined.
    params.repeat = params.repeat || false;

    return params;
  }

  doKeyDown(evt) {
    return () => {
      this.getLearnModeWindow().KbExplorer.onKeyDown(
          this.makeMockKeyEvent(evt));
    };
  }

  doKeyUp(evt) {
    return () => {
      this.getLearnModeWindow().KbExplorer.onKeyUp(this.makeMockKeyEvent(evt));
    };
  }

  doGesture(gesture) {
    return () => {
      this.getLearnModeWindow().KbExplorer.onAccessibilityGesture(gesture);
    };
  }

  doBrailleKeyEvent(evt) {
    return () => {
      this.getLearnModeWindow().KbExplorer.onBrailleKeyEvent(evt);
    };
  }
};

TEST_F('ChromeVoxLearnModeTest', 'KeyboardInput', function() {
  this.runOnLearnModePage((mockFeedback, evt) => {
    // Press Search+Right.
    mockFeedback.call(doKeyDown({keyCode: KeyCode.SEARCH, metaKey: true}))
        .expectSpeechWithQueueMode('Search', QueueMode.FLUSH)
        .call(doKeyDown({keyCode: KeyCode.RIGHT, metaKey: true}))
        .expectSpeechWithQueueMode('Right arrow', QueueMode.QUEUE)
        .expectSpeechWithQueueMode('Next Object', QueueMode.QUEUE)
        .call(doKeyUp({keyCode: KeyCode.RIGHT, metaKey: true}))

        // Hit 'Right' again. We should get flushed output.
        .call(doKeyDown({keyCode: KeyCode.RIGHT, metaKey: true}))
        .expectSpeechWithQueueMode('Right arrow', QueueMode.FLUSH)
        .expectSpeechWithQueueMode('Next Object', QueueMode.QUEUE)
        .call(doKeyUp({keyCode: KeyCode.RIGHT, metaKey: true}))

        .replay();
  });
});

TEST_F('ChromeVoxLearnModeTest', 'KeyboardInputRepeat', function() {
  this.runOnLearnModePage((mockFeedback, evt) => {
    // Press Search repeatedly.
    mockFeedback.call(doKeyDown({keyCode: KeyCode.SEARCH, metaKey: true}))
        .expectSpeechWithQueueMode('Search', QueueMode.FLUSH)

        // This in theory should never happen (no repeat set).
        .call(doKeyDown({keyCode: KeyCode.SEARCH, metaKey: true}))
        .expectSpeechWithQueueMode('Search', QueueMode.QUEUE)

        // Hit Search again with the right underlying data. Then hit Control to
        // generate some speech.
        .call(doKeyDown({keyCode: KeyCode.SEARCH, metaKey: true, repeat: true}))
        .call(doKeyDown({keyCode: KeyCode.CONTROL, ctrlKey: true}))
        .expectNextSpeechUtteranceIsNot('Search')
        .expectSpeechWithQueueMode('Control', QueueMode.QUEUE)

        .replay();
  });
});

TEST_F('ChromeVoxLearnModeTest', 'Gesture', function() {
  this.runOnLearnModePage((mockFeedback, evt) => {
    this.getLearnModeWindow().KbExplorer.MIN_TOUCH_EXPLORE_OUTPUT_TIME_MS_ = 0;
    mockFeedback.call(doGesture('swipeRight1'))
        .expectSpeechWithQueueMode('Swipe one finger right', QueueMode.FLUSH)
        .expectSpeechWithQueueMode('Next Object', QueueMode.QUEUE)

        .call(doGesture('swipeLeft1'))
        .expectSpeechWithQueueMode('Swipe one finger left', QueueMode.FLUSH)
        .expectSpeechWithQueueMode('Previous Object', QueueMode.QUEUE)

        .call(doGesture('touchExplore'))
        .expectSpeechWithQueueMode('Touch explore', QueueMode.FLUSH)

        // Test for inclusion of commandDescriptionMsgId when provided.
        .call(doGesture('swipeLeft2'))
        .expectSpeechWithQueueMode('Swipe two fingers left', QueueMode.FLUSH)
        .expectSpeechWithQueueMode('Escape', QueueMode.QUEUE)

        .replay();
  });
});

TEST_F('ChromeVoxLearnModeTest', 'Braille', function() {
  this.runOnLearnModePage((mockFeedback, evt) => {
    // Hit the left panning hardware key on a braille display.
    mockFeedback.call(doBrailleKeyEvent({command: BrailleKeyCommand.PAN_LEFT}))
        .expectSpeechWithQueueMode('Pan backward', QueueMode.FLUSH)
        .expectBraille('Pan backward')

        // Hit the backspace chord on a Perkins braille keyboard (aka dot 7).
        .call(doBrailleKeyEvent(
            {command: BrailleKeyCommand.CHORD, brailleDots: 0b1000000}))
        .expectSpeechWithQueueMode('Backspace', QueueMode.FLUSH)
        .expectBraille('Backspace')

        // Hit a 'd' chord (Perkins keys dot 1-4-5).
        .call(doBrailleKeyEvent(
            {command: BrailleKeyCommand.CHORD, brailleDots: 0b011001}))
        .expectSpeechWithQueueMode('dots 1-4-5 chord', QueueMode.FLUSH)
        .expectBraille('dots 1-4-5 chord')

        .replay();
  });
});

TEST_F('ChromeVoxLearnModeTest', 'HardwareFunctionKeys', function() {
  this.runOnLearnModePage((mockFeedback, evt) => {
    mockFeedback.call(doKeyDown({keyCode: KeyCode.BRIGHTNESS_UP}))
        .expectSpeechWithQueueMode('Brightness up', QueueMode.FLUSH)
        .call(doKeyUp({keyCode: KeyCode.BRIGHTNESS_UP}))

        .call(doKeyDown({keyCode: KeyCode.SEARCH, metaKey: true}))
        .expectSpeechWithQueueMode('Search', QueueMode.FLUSH)
        .call(doKeyDown({keyCode: KeyCode.BRIGHTNESS_UP, metaKey: true}))
        .expectSpeechWithQueueMode('Brightness up', QueueMode.QUEUE)
        .expectSpeechWithQueueMode('Toggle dark screen', QueueMode.QUEUE)
        .call(doKeyUp({keyCode: KeyCode.BRIGHTNESS_UP, metaKey: true}))

        // Search+Volume Down has no associated command.
        .call(doKeyDown({keyCode: KeyCode.VOLUME_DOWN, metaKey: true}))
        .expectSpeechWithQueueMode('volume down', QueueMode.FLUSH)
        .call(doKeyUp({keyCode: KeyCode.VOLUME_DOWN, metaKey: true}))

        // Search+Volume Mute does though.
        .call(doKeyDown({keyCode: KeyCode.VOLUME_MUTE, metaKey: true}))
        .expectSpeechWithQueueMode('volume mute', QueueMode.FLUSH)
        .expectSpeechWithQueueMode('Toggle speech on or off', QueueMode.QUEUE)

        .replay();
  });
});
