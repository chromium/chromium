// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../testing/chromevox_e2e_test_base.js']);

GEN_INCLUDE(['../testing/fake_objects.js']);

/**
 * Test fixture for DesktopAutomationHandler.
 */
ChromeVoxDesktopAutomationHandlerTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    // Alphabetical based on file path.
    await importModule(
        'ChromeVoxState', '/chromevox/background/chromevox_state.js');
    await importModule(
        'DesktopAutomationHandler',
        '/chromevox/background/desktop_automation_handler.js');
    await importModule(
        'DesktopAutomationInterface',
        '/chromevox/background/desktop_automation_interface.js');
    await importModule(
        'CustomAutomationEvent',
        '/chromevox/common/custom_automation_event.js');
    await importModule('EventGenerator', '/common/event_generator.js');
    await importModule('KeyCode', '/common/key_code.js');

    await ChromeVoxState.ready();
    this.handler_ = DesktopAutomationInterface.instance;

    globalThis.EventType = chrome.automation.EventType;
    globalThis.RoleType = chrome.automation.RoleType;
    globalThis.StateType = chrome.automation.StateType;

    globalThis.press = this.press;
  }

  press(keyCode, modifiers) {
    return function() {
      EventGenerator.sendKeyPress(keyCode, modifiers);
    };
  }
};

AX_TEST_F(
    'ChromeVoxDesktopAutomationHandlerTest', 'OnValueChangedSlider',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `<input type="range"></input>`;
      const root = await this.runWithLoadedTree(site);
      const slider = root.find({role: RoleType.SLIDER});
      assertTrue(Boolean(slider));

      let sliderValue = '50%';
      Object.defineProperty(slider, 'value', {get: () => sliderValue});

      const event = new CustomAutomationEvent(EventType.VALUE_CHANGED, slider);
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
              () => DesktopAutomationHandler.MIN_VALUE_CHANGE_DELAY_MS = 10000)
          .call(() => sliderValue = '70%')
          .call(() => this.handler_.onValueChanged(event))

          // Send one more that is processed.
          .call(() => DesktopAutomationHandler.MIN_VALUE_CHANGE_DELAY_MS = -1)
          .call(() => sliderValue = '80%')
          .call(() => this.handler_.onValueChanged(event))

          .expectNextSpeechUtteranceIsNot('70%')
          .expectSpeech('80%');

      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxDesktopAutomationHandlerTest', 'OnAutofillAvailabilityChanged',
    async function() {
      const AUTOFILL_AVAILABLE_UTTERANCE =
          'Press up or down arrow for auto completions';
      const root = await this.runWithLoadedTree(`<input><button>`);
      const input = root.find({role: RoleType.TEXT_FIELD});
      const button = root.find({role: RoleType.BUTTON});
      const state =
          {[StateType.FOCUSED]: false, [StateType.AUTOFILL_AVAILABLE]: false};
      Object.defineProperty(input, 'state', {get: () => state});

      const event = new CustomAutomationEvent(
          EventType.AUTOFILL_AVAILABILITY_CHANGED, input);
      const utterances = [];
      ChromeVox.tts.speak = utterances.push.bind(utterances);

      // Autofill available, but it is not focused: no feedback expected
      state[StateType.FOCUSED] = false;
      state[StateType.AUTOFILL_AVAILABLE] = true;
      this.handler_.onAutofillAvailabilityChanged(event);
      assertEquals(utterances.indexOf(AUTOFILL_AVAILABLE_UTTERANCE), -1);

      // Focused element with no autofill availability: no feedback
      state[StateType.FOCUSED] = true;
      state[StateType.AUTOFILL_AVAILABLE] = false;
      this.handler_.onAutofillAvailabilityChanged(event);
      assertEquals(utterances.indexOf(AUTOFILL_AVAILABLE_UTTERANCE), -1);

      // Focused element receives autofill options: announce it
      state[StateType.FOCUSED] = true;
      state[StateType.AUTOFILL_AVAILABLE] = true;
      this.handler_.onAutofillAvailabilityChanged(event);
      assertNotEquals(utterances.indexOf(AUTOFILL_AVAILABLE_UTTERANCE), -1);

      const mockFeedback = this.createMockFeedback();
      mockFeedback
          .call(() => {
            // Get focus on element with autofill: it should be announced
            button.focus();
            input.focus();
          })
          .expectSpeech(AUTOFILL_AVAILABLE_UTTERANCE);

      await mockFeedback.replay();
    });

TEST_F(
    'ChromeVoxDesktopAutomationHandlerTest', 'TaskManagerTableView',
    function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedDesktop(desktop => {
        mockFeedback
            .call(() => {
              EventGenerator.sendKeyPress(KeyCode.ESCAPE, {search: true});
            })
            .expectSpeech('Task Manager, window')
            .call(() => {
              EventGenerator.sendKeyPress(KeyCode.DOWN);
            })
            .expectSpeech('Browser', 'row 2 column 1', 'Task')
            .call(() => {
              EventGenerator.sendKeyPress(KeyCode.DOWN);
            })
            // Make sure it doesn't repeat the previous line!
            .expectNextSpeechUtteranceIsNot('Browser')
            .expectSpeech('row 3 column 1')

            .replay();
      });
    });

// Ensures behavior when IME candidates are selected.
AX_TEST_F(
    'ChromeVoxDesktopAutomationHandlerTest', 'ImeCandidate', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `<button>First</button><button>Second</button>`;
      const root = await this.runWithLoadedTree(site);
      const candidates = root.findAll({role: RoleType.BUTTON});
      const first = candidates[0];
      const second = candidates[1];
      assertNotNullNorUndefined(first);
      assertNotNullNorUndefined(second);
      // Fake roles to imitate IME candidates.
      Object.defineProperty(first, 'role', {get: () => RoleType.IME_CANDIDATE});
      Object.defineProperty(
          second, 'role', {get: () => RoleType.IME_CANDIDATE});
      const selectFirst = new CustomAutomationEvent(EventType.SELECTION, first);
      const selectSecond =
          new CustomAutomationEvent(EventType.SELECTION, second);
      mockFeedback.call(() => this.handler_.onSelection(selectFirst))
          .expectSpeech('First')
          .expectSpeech('F: foxtrot, i: india, r: romeo, s: sierra, t: tango')
          .call(() => this.handler_.onSelection(selectSecond))
          .expectSpeech('Second')
          .expectSpeech(
              'S: sierra, e: echo, c: charlie, o: oscar, n: november, d: delta')
          .call(() => this.handler_.onSelection(selectFirst))
          .expectSpeech('First')
          .expectSpeech(/foxtrot/);
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxDesktopAutomationHandlerTest', 'IgnoreRepeatedAlerts',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `<button>Hello world</button>`;
      const root = await this.runWithLoadedTree(site);
      const button = root.find({role: RoleType.BUTTON});
      assertTrue(Boolean(button));
      const event = new CustomAutomationEvent(EventType.ALERT, button);
      mockFeedback
          .call(() => {
            DesktopAutomationHandler.MIN_ALERT_DELAY_MS = 20 * 1000;
            this.handler_.onAlert(event);
          })
          .expectSpeech('Hello world')
          .clearPendingOutput()
          .call(() => {
            // Repeated alerts should be ignored.
            this.handler_.onAlert(event);
            assertFalse(mockFeedback.utteranceInQueue('Hello world'));
            this.handler_.onAlert(event);
            assertFalse(mockFeedback.utteranceInQueue('Hello world'));
          });
      await mockFeedback.replay();
    });

// TODO(crbug.com/1292501): Fix flakiness.
AX_TEST_F(
    'ChromeVoxDesktopAutomationHandlerTest', 'DISABLED_DatalistSelection',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <input aria-label="Choose one" list="list">
    <datalist id="list">
    <option>foo</option>
    <option>bar</option>
    </datalist>
  `;
      const root = await this.runWithLoadedTree(site);
      const combobox = root.find({
        role: RoleType.TEXT_FIELD_WITH_COMBO_BOX,
        attributes: {name: 'Choose one'},
      });
      assertTrue(Boolean(combobox));
      combobox.focus();
      await new Promise(r => combobox.addEventListener(EventType.FOCUS, r));

      // The combobox is now actually focused, safe to send arrows.
      mockFeedback.call(press(KeyCode.DOWN))
          .expectSpeech('foo', 'List item', ' 1 of 2 ')
          .expectBraille('foo lstitm 1/2 (x)')
          .call(press(KeyCode.DOWN))
          .expectSpeech('bar', 'List item', ' 2 of 2 ')
          .expectBraille('bar lstitm 2/2 (x)')
          .call(press(KeyCode.UP))
          .expectSpeech('foo', 'List item', ' 1 of 2 ')
          .expectBraille('foo lstitm 1/2 (x)');
      await mockFeedback.replay();
    });
