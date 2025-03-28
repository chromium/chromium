// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '../testing/chromevox_e2e_test_base.js',
]);

/**
 * Test fixture for ChromeVox Learn Mode page.
 */
ChromeVoxLearnModeTest = class extends ChromeVoxE2ETest {
  constructor() {
    super();
    globalThis.EventType = chrome.automation.EventType;
    globalThis.Gesture = chrome.accessibilityPrivate.Gesture;

    globalThis.doKeyDown = this.doKeyDown.bind(this);
    globalThis.doKeyUp = this.doKeyUp.bind(this);
    globalThis.doLearnModeGesture = this.doLearnModeGesture.bind(this);
    globalThis.doBrailleKeyEvent = this.doBrailleKeyEvent.bind(this);
  }

  async runOnLearnModePage() {
    return new Promise(async resolve => {
      const mockFeedback = this.createMockFeedback();
      const desktop = await AsyncUtil.getDesktop();
      function listener(evt) {
        if (evt.target.docUrl.indexOf('learn_mode/learn_mode.html') === -1 ||
            !evt.target.docLoaded) {
          return;
        }
        desktop.removeEventListener(
            chrome.automation.EventType.LOAD_COMPLETE, listener);

        mockFeedback.expectSpeech(/Press a qwerty key/);
        resolve([mockFeedback, evt]);
      }

      desktop.addEventListener(
          chrome.automation.EventType.LOAD_COMPLETE, listener);
      CommandHandlerInterface.instance.onCommand('showLearnModePage');
    });
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
    return async () =>
               await LearnModeBridge.onKeyDown(this.makeMockKeyEvent(evt));
  }

  doKeyUp(evt) {
    return async () =>
               await LearnModeBridge.onKeyUp(this.makeMockKeyEvent(evt));
  }

  doLearnModeGesture(gesture) {
    return async () => await LearnModeBridge.onAccessibilityGesture(gesture);
  }

  doBrailleKeyEvent(evt) {
    return async () => await LearnModeBridge.onBrailleKeyEvent(evt);
  }
};

// TODO(crbug.com/1128926, crbug.com/1172387):
// Test times out flakily.
AX_TEST_F('ChromeVoxLearnModeTest', 'DISABLED_KeyboardInput', async function() {
  const [mockFeedback, evt] = await this.runOnLearnModePage();
  // Press Search+Right.
  mockFeedback.call(doKeyDown({keyCode: KeyCode.SEARCH, metaKey: true}))
      .expectSpeechWithQueueMode('Search', QueueMode.CATEGORY_FLUSH)
      .call(doKeyDown({keyCode: KeyCode.RIGHT, metaKey: true}))
      .expectSpeechWithQueueMode('Right arrow', QueueMode.QUEUE)
      .expectSpeechWithQueueMode('Next Object', QueueMode.QUEUE)
      .call(doKeyUp({keyCode: KeyCode.RIGHT, metaKey: true}))

      // Hit 'Right' again. We should get flushed output.
      .call(doKeyDown({keyCode: KeyCode.RIGHT, metaKey: true}))
      .expectSpeechWithQueueMode('Right arrow', QueueMode.CATEGORY_FLUSH)
      .expectSpeechWithQueueMode('Next Object', QueueMode.QUEUE)
      .call(doKeyUp({keyCode: KeyCode.RIGHT, metaKey: true}));

  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxLearnModeTest', 'KeyboardInputRepeat', async function() {
  const [mockFeedback, evt] = await this.runOnLearnModePage();
  // Press Search repeatedly.
  mockFeedback.call(doKeyDown({keyCode: KeyCode.SEARCH, metaKey: true}))
      .expectSpeechWithQueueMode('Search', QueueMode.CATEGORY_FLUSH)

      // This in theory should never happen (no repeat set).
      .call(doKeyDown({keyCode: KeyCode.SEARCH, metaKey: true}))
      .expectSpeechWithQueueMode('Search', QueueMode.QUEUE)

      // Hit Search again with the right underlying data. Then hit Control to
      // generate some speech.
      .call(doKeyDown({keyCode: KeyCode.SEARCH, metaKey: true, repeat: true}))
      .call(doKeyDown({keyCode: KeyCode.CONTROL, ctrlKey: true}))
      .expectNextSpeechUtteranceIsNot('Search')
      .expectSpeechWithQueueMode('Control', QueueMode.QUEUE);

  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxLearnModeTest', 'Gesture', async function() {
  const [mockFeedback, evt] = await this.runOnLearnModePage();
  await LearnModeBridge.clearTouchExploreOutputTime();
  mockFeedback.call(doLearnModeGesture(Gesture.SWIPE_RIGHT1))
      .expectSpeechWithQueueMode(
          'Swipe one finger right', QueueMode.CATEGORY_FLUSH)
      .expectSpeechWithQueueMode('Next Object', QueueMode.QUEUE)

      .call(doLearnModeGesture(Gesture.SWIPE_LEFT1))
      .expectSpeechWithQueueMode(
          'Swipe one finger left', QueueMode.CATEGORY_FLUSH)
      .expectSpeechWithQueueMode('Previous Object', QueueMode.QUEUE)

      .call(doLearnModeGesture(Gesture.TOUCH_EXPLORE))
      .expectSpeechWithQueueMode('Touch explore', QueueMode.CATEGORY_FLUSH)

      // Test for inclusion of commandDescriptionMsgId when provided.
      .call(doLearnModeGesture(Gesture.SWIPE_RIGHT2))
      .expectSpeechWithQueueMode(
          'Swipe two fingers right', QueueMode.CATEGORY_FLUSH)
      .expectSpeechWithQueueMode('Enter', QueueMode.QUEUE);

  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxLearnModeTest', 'Braille', async function() {
  const [mockFeedback, evt] = await this.runOnLearnModePage();
  // Hit the left panning hardware key on a braille display.
  mockFeedback.call(doBrailleKeyEvent({command: BrailleKeyCommand.PAN_LEFT}))
      .expectSpeechWithQueueMode('Pan backward', QueueMode.CATEGORY_FLUSH)
      .expectBraille('Pan backward')

      // Hit the backspace chord on a Perkins braille keyboard (aka dot 7).
      .call(doBrailleKeyEvent(
          {command: BrailleKeyCommand.CHORD, brailleDots: 0b1000000}))
      .expectSpeechWithQueueMode('Backspace', QueueMode.CATEGORY_FLUSH)
      .expectBraille('Backspace')

      // Hit a 'd' chord (Perkins keys dot 1-4-5).
      .call(doBrailleKeyEvent(
          {command: BrailleKeyCommand.CHORD, brailleDots: 0b011001}))
      .expectSpeechWithQueueMode('dots 1-4-5 chord', QueueMode.CATEGORY_FLUSH)
      .expectBraille('dots 1-4-5 chord');

  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxLearnModeTest', 'HardwareFunctionKeys', async function() {
  const [mockFeedback, evt] = await this.runOnLearnModePage();
  mockFeedback.call(doKeyDown({keyCode: KeyCode.BRIGHTNESS_UP}))
      .expectSpeechWithQueueMode('Brightness up', QueueMode.CATEGORY_FLUSH)
      .call(doKeyUp({keyCode: KeyCode.BRIGHTNESS_UP}))

      .call(doKeyDown({keyCode: KeyCode.SEARCH, metaKey: true}))
      .expectSpeechWithQueueMode('Search', QueueMode.CATEGORY_FLUSH)
      .call(doKeyDown({keyCode: KeyCode.BRIGHTNESS_UP, metaKey: true}))
      .expectSpeechWithQueueMode('Brightness up', QueueMode.QUEUE)
      .expectSpeechWithQueueMode('Toggle screen on or off', QueueMode.QUEUE)
      .call(doKeyUp({keyCode: KeyCode.BRIGHTNESS_UP, metaKey: true}))

      // Search+Volume Down has no associated command.
      .call(doKeyDown({keyCode: KeyCode.VOLUME_DOWN, metaKey: true}))
      .expectSpeechWithQueueMode('volume down', QueueMode.CATEGORY_FLUSH)
      .call(doKeyUp({keyCode: KeyCode.VOLUME_DOWN, metaKey: true}))

      // Search+Volume Mute does though.
      .call(doKeyDown({keyCode: KeyCode.VOLUME_MUTE, metaKey: true}))
      .expectSpeechWithQueueMode('volume mute', QueueMode.CATEGORY_FLUSH)
      .expectSpeechWithQueueMode('Toggle speech on or off', QueueMode.QUEUE);

  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxLearnModeTest', 'CommandHandlersDisabled', async function() {
      const [mockFeedback, evt] = await this.runOnLearnModePage();
      await LearnModeBridge.ready();
      assertTrue(BrailleCommandHandler.instance.bypassed_);
      assertTrue(GestureCommandHandler.instance.bypassed_);
      await mockFeedback.replay();
    });
