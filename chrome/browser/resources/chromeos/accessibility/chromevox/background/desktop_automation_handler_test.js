// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../testing/chromevox_next_e2e_test_base.js']);

GEN_INCLUDE(['../testing/fake_objects.js']);

/**
 * Test fixture for DesktopAutomationHandler.
 */
ChromeVoxDesktopAutomationHandlerTest = class extends ChromeVoxNextE2ETest {
  /** @override */
  setUp() {
    super.setUp();

    const runTest = this.deferRunTest(WhenTestDone.EXPECT);
    chrome.automation.getDesktop(desktop => {
      this.handler_ = new DesktopAutomationHandler(desktop);
      runTest();
    });
  }
};

TEST_F(
    'ChromeVoxDesktopAutomationHandlerTest', 'OnValueChangedSlider',
    function() {
      const mockFeedback = this.createMockFeedback();
      const site = `<input type="range"></input>`;
      this.runWithLoadedTree(site, function(root) {
        const slider = root.find({role: RoleType.SLIDER});
        assertTrue(!!slider);

        let sliderValue = '50%';
        Object.defineProperty(slider, 'value', {get: () => sliderValue});

        const event =
            new CustomAutomationEvent(EventType.VALUE_CHANGED, slider);
        mockFeedback.call(() => this.handler_.onValueChanged(event))
            .expectSpeech('Slider', '50%')

            // Override the min time to observe value changes so that even super
            // fast updates triggers speech.
            .call(() => DesktopAutomationHandler.MIN_VALUE_CHANGE_DELAY_MS = -1)
            .call(() => sliderValue = '60%')
            .call(() => this.handler_.onValueChanged(event))

            // The range stays on the slider, so subsequent value changes only
            // report the value.
            .expectNextSpeechUtteranceIsNot('Slider')
            .expectSpeech('60%')

            // Set the min time and send a value change which should be ignored.
            .call(
                () => DesktopAutomationHandler.MIN_VALUE_CHANGE_DELAY_MS =
                    10000)
            .call(() => sliderValue = '70%')
            .call(() => this.handler_.onValueChanged(event))

            // Send one more that is processed.
            .call(() => DesktopAutomationHandler.MIN_VALUE_CHANGE_DELAY_MS = -1)
            .call(() => sliderValue = '80%')
            .call(() => this.handler_.onValueChanged(event))

            .expectNextSpeechUtteranceIsNot('70%')
            .expectSpeech('80%')

            .replay();
      });
    });
