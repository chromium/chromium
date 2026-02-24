// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);
GEN_INCLUDE(['../../../../common/testing/documents.js']);
GEN_INCLUDE(['../../testing/fake_objects.js']);

/**
 * Test fixture for DesktopAutomationHandler.
 */
ChromeVoxDesktopAutomationHandlerTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

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

  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
  #include "chrome/browser/task_manager/common/task_manager_features.h"
      `);
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
      mockFeedback.call(() => this.handler_.onValueChanged_(event))
          .expectSpeech('Slider', '50%')

          // Override the min time to observe value changes so that even super
          // fast updates triggers speech.
          .call(() => DesktopAutomationHandler.MIN_VALUE_CHANGE_DELAY_MS = -1)
          .call(() => sliderValue = '60%')
          .call(() => this.handler_.onValueChanged_(event))

          // The range stays on the slider, so subsequent value changes only
          // report the value.
          .expectNextSpeechUtteranceIsNot('Slider')
          .expectSpeech('60%')

          // Set the min time and send a value change which should be ignored.
          .call(
              () => DesktopAutomationHandler.MIN_VALUE_CHANGE_DELAY_MS = 10000)
          .call(() => sliderValue = '70%')
          .call(() => this.handler_.onValueChanged_(event))

          // Send one more that is processed.
          .call(() => DesktopAutomationHandler.MIN_VALUE_CHANGE_DELAY_MS = -1)
          .call(() => sliderValue = '80%')
          .call(() => this.handler_.onValueChanged_(event))

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

// Ensures that selection events from IME candidate doesn't break ChromeVox's
// range.
AX_TEST_F(
    'ChromeVoxDesktopAutomationHandlerTest', 'ImeCandidate_keepRange',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const site =
          `<button>First</button><button>Second</button><button>Third</button>`;
      const root = await this.runWithLoadedTree(site);
      const candidates = root.findAll({role: RoleType.BUTTON});
      const first = candidates[0];
      const third = candidates[2];
      assertNotNullNorUndefined(first);
      assertNotNullNorUndefined(third);
      // Fake role to imitate IME candidates.
      Object.defineProperty(third, 'role', {get: () => RoleType.IME_CANDIDATE});
      const selectEvent = new CustomAutomationEvent(EventType.SELECTION, third);

      mockFeedback.call(() => first.focus())
          .expectSpeech('First')
          .call(() => this.handler_.onSelection(selectEvent))
          .expectSpeech('Third')
          .expectSpeech(/tango/)
          .call(doCmd('nextObject'))
          .expectSpeech('Second');
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
            this.handler_.onAlert_(event);
          })
          .expectSpeech('Hello world')
          .clearPendingOutput()
          .call(() => {
            // Repeated alerts should be ignored.
            this.handler_.onAlert_(event);
            assertFalse(mockFeedback.utteranceInQueue('Hello world'));
            this.handler_.onAlert_(event);
            assertFalse(mockFeedback.utteranceInQueue('Hello world'));
          });
      await mockFeedback.replay();
    });

// TODO(crbug.com/40819389): Fix flakiness.
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

AX_TEST_F(
    'ChromeVoxDesktopAutomationHandlerTest', 'OnDocumentSelectionChanged',
    async function() {
      const root = await this.runWithLoadedTree(`
          <div>
            <input type="text" value="I’m Nobody! Who are you?"></input>
          </div>
          <p>The first line of a poem by Emily Dickinson.<p>
          `);
      const input = root.find({role: RoleType.TEXT_FIELD});
      assertNotNullNorUndefined(input);
      const text =
          root.find({role: RoleType.STATIC_TEXT, state: {editable: false}});
      assertNotNullNorUndefined(text);
      assertTrue(text.name.includes('Emily Dickinson'));
      const instance = DesktopAutomationHandler.instance;

      // Verify that onEditableChanged_ is called.
      let called = false;
      this.addCallbackPostMethod(
          instance, 'onEditableChanged_', () => called = true);

      // Case: editable with valid start and end.
      const promise =
          this.waitForEvent(instance.node_, 'documentSelectionChanged', true);
      chrome.automation.setDocumentSelection({
        anchorObject: input,
        anchorOffset: 0,
        focusObject: input,
        focusOffset: 7,
      });
      await promise;

      assertTrue(called);
      called = false;

      // Case: no selection start.
      // Because automation.setDocumentSelection enforces that there is a
      // selectionStart object, we will call the method directly.
      instance.onDocumentSelectionChanged_({
        target: {
          selectionStartObject: null,
          selectionStartOffset: 0,
          selectionEndObject: input,
          selectionEndOffset: 3,
        },
      });

      assertFalse(called);
    });

AX_TEST_F('ChromeVoxDesktopAutomationHandlerTest', 'OnFocus', async function() {
  const root = await this.runWithLoadedTree(Documents.button);
  const button = root.find({role: RoleType.BUTTON});

  // Case 1: Exits early if it's a rootWebArea that's not a frame, and the event
  // is not from an action.
  assertEquals('rootWebArea', root.role);
  // Ensure it appears to not be a frame.
  const rootParent = root.parent;
  Object.defineProperty(root, 'parent', {value: null, configurable: true});
  // The textEditHandler_ should not change if we exit early.
  DesktopAutomationInterface.instance.textEditHandler_ = 'fake handler';
  const assertGetFocusCalled = this.prepareToExpectMethodCall(
      DesktopAutomationInterface.instance, 'maybeRecoverFocusAndOutput_');

  DesktopAutomationInterface.instance.onFocus_({target: root});
  await this.waitForPendingMethods();
  assertGetFocusCalled();
  assertEquals(
      'fake handler', DesktopAutomationInterface.instance.textEditHandler_);

  Object.defineProperty(root, 'parent', {value: rootParent});

  const assertNoOutput = async () => {
    const assertCreateTextHandlerCalled = this.prepareToExpectMethodCall(
        DesktopAutomationInterface.instance, 'createTextEditHandlerIfNeeded_');
    const assertExitEarly = this.prepareToExpectMethodNotCalled(
        Output, 'forceModeForNextSpeechUtterance');

    DesktopAutomationInterface.instance.onFocus_({target: button});
    await this.waitForPendingMethods();
    assertCreateTextHandlerCalled();
    assertExitEarly();
  };

  // Case 2: Ignore embedded objects.
  Object.defineProperty(
      button, 'role', {value: RoleType.EMBEDDED_OBJECT, configurable: true});
  await assertNoOutput();

  // Case 3: Ignore plugin objects.
  Object.defineProperty(
      button, 'role', {value: RoleType.PLUGIN_OBJECT, configurable: true});
  await assertNoOutput();

  // Case 4: Ignore web views.
  Object.defineProperty(
      button, 'role', {value: RoleType.WEB_VIEW, configurable: true});
  await assertNoOutput();

  // Case 5: Ignore nodes with unknown role if there's not a reasonable target
  // to "sync down" into.
  AutomationUtil.findNodePre = () => null;
  Object.defineProperty(
      button, 'role', {value: RoleType.UNKNOWN, configurable: true});
  await assertNoOutput();

  Object.defineProperty(button, 'role', {value: RoleType.BUTTON});

  // Case 6: Ignore nodes with no root.
  Object.defineProperty(button, 'root', {value: null, configurable: true});
  await assertNoOutput();

  Object.defineProperty(button, 'root', {value: root});

  // Case 7: AutoScrollHandler eats the event with onFocusEventNavigation().
  AutoScrollHandler.instance.onFocusEventNavigation = () => false;
  await assertNoOutput();

  AutoScrollHandler.instance.onFocusEventNavigation = () => true;

  // Default case.
  DesktopAutomationInterface.instance.lastRootUrl_ = 'fake url';
  const assertOutputFlush =
      this.prepareToExpectMethodCall(Output, 'forceModeForNextSpeechUtterance');
  const assertEventDefault = this.prepareToExpectMethodCall(
      DesktopAutomationInterface.instance, 'onEventDefault');

  DesktopAutomationInterface.instance.onFocus_({target: button});
  await this.waitForPendingMethods();
  assertNotEquals('fake url', DesktopAutomationInterface.instance.lastRootUrl_);
  assertOutputFlush();
  assertEventDefault();
});

AX_TEST_F(
    'ChromeVoxDesktopAutomationHandlerTest',
    'NoActiononMenuItemCollapsedWhenNoMenuPopupStart', async function() {
      const site = `
        <ol role="menu">
          <li role="menuitem" aria-expanded="false" id="1">
          </li>
        </ol>`;
      const root = await this.runWithLoadedTree(site);
      const menuItem = root.find({role: RoleType.MENU_ITEM});
      assertTrue(Boolean(menuItem));
      const state = {[StateType.COLLAPSED]: false, [StateType.EXPANDED]: true};
      Object.defineProperty(menuItem, 'state', {get: () => state});
      Object.defineProperty(menuItem, 'selected', {get: () => true});
      assertTrue(menuItem.state.expanded);
      assertFalse(DesktopAutomationInterface.instance.isSubMenuShowing_);

      const assertNoEventDefault = this.prepareToExpectMethodNotCalled(
          DesktopAutomationInterface.instance, 'onEventDefault');
      state[StateType.COLLAPSED] = true;
      state[StateType.EXPANDED] = false;
      const collapsedEvent =
          new CustomAutomationEvent(EventType.COLLAPSED, menuItem);
      this.handler_.onMenuItemCollapsed(collapsedEvent);
      await this.waitForPendingMethods();
      assertTrue(menuItem.state.collapsed);
      assertTrue(menuItem.selected);
      assertNoEventDefault();
    });

AX_TEST_F(
    'ChromeVoxDesktopAutomationHandlerTest',
    'onMenuItemCollapsedAfterMenuPopupStart', async function() {
      const site = `
      <ol role="menu">
        <li role="menuitem" aria-expanded="false" id="1">
        </li>
      </ol>`;
      const root = await this.runWithLoadedTree(site);
      const menu = root.find({role: RoleType.MENU});
      const menuItem = root.find({role: RoleType.MENU_ITEM});
      assertTrue(Boolean(menu));
      assertTrue(Boolean(menuItem));

      const state = {[StateType.COLLAPSED]: false, [StateType.EXPANDED]: true};
      Object.defineProperty(menuItem, 'state', {get: () => state});
      Object.defineProperty(menuItem, 'selected', {get: () => true});
      const event = new CustomAutomationEvent(EventType.MENU_POPUP_START, menu);
      this.handler_.onMenuPopupStart_(event);
      assertTrue(DesktopAutomationInterface.instance.isSubMenuShowing_);

      const assertEventDefault = this.prepareToExpectMethodCall(
          DesktopAutomationInterface.instance, 'onEventDefault');
      state[StateType.COLLAPSED] = true;
      state[StateType.EXPANDED] = false;
      const collapsedEvent =
          new CustomAutomationEvent(EventType.COLLAPSED, menuItem);
      this.handler_.onMenuItemCollapsed(collapsedEvent);
      await this.waitForPendingMethods();
      assertTrue(menuItem.state.collapsed);
      assertTrue(menuItem.selected);
      assertEventDefault();
    });

AX_TEST_F(
    'ChromeVoxDesktopAutomationHandlerTest',
    'ResetIsSubMenuShowingStateWhenFocusChanges', async function() {
      const site = `
      <ol role="menu">
        <li role="menuitem" aria-expanded="false" id="1">
        </li>
      </ol>
      <button>`;
      const root = await this.runWithLoadedTree(site);
      const menu = root.find({role: RoleType.MENU});
      const menuItem = root.find({role: RoleType.MENU_ITEM});
      const button = root.find({role: RoleType.BUTTON});
      assertTrue(Boolean(menu));
      assertTrue(Boolean(menuItem));
      assertTrue(Boolean(button));

      const state = {[StateType.COLLAPSED]: false, [StateType.EXPANDED]: true};
      Object.defineProperty(menuItem, 'state', {get: () => state});
      Object.defineProperty(menuItem, 'selected', {get: () => true});
      const event = new CustomAutomationEvent(EventType.MENU_POPUP_START, menu);
      this.handler_.onMenuPopupStart_(event);
      assertTrue(DesktopAutomationInterface.instance.isSubMenuShowing_);

      const focusEvent = new CustomAutomationEvent(EventType.FOCUS, button);
      this.handler_.onFocus_(focusEvent);
      await this.waitForPendingMethods();

      assertFalse(DesktopAutomationInterface.instance.isSubMenuShowing_);
    });

/**
 * Test fixture for DesktopAutomationHandler without TaskManagerDesktopRefresh.
 * `features::kTaskManagerDesktopRefresh` is included in the base fixture which
 * inherits from ChromeVoxE2ETest.
 */
ChromeVoxDesktopAutomationHandlerWithoutTaskManagerDesktopRefreshTest =
    class extends ChromeVoxDesktopAutomationHandlerTest {
  /** @override */
  get featureList() {
    let list = super.featureList || {};
    list.disabled = list.disabled || [];
    list.disabled.push('features::kTaskManagerDesktopRefresh');
    return list;
  }
};

TEST_F(
    'ChromeVoxDesktopAutomationHandlerWithoutTaskManagerDesktopRefreshTest',
    'TaskManagerTableView', function() {
      // TODO(crbug.com/397484647): Merge this test with the
      // TaskManagerDesktopRefresh version, and reintegrate it back into
      // ChromeVoxDesktopAutomationHandlerTest after the flag is enabled by
      // default.

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
            .expectSpeech('Browser', /row [0-9]+ column 1/, 'Task')
            .call(() => {
              EventGenerator.sendKeyPress(KeyCode.DOWN);
            })
            // Make sure it doesn't repeat the previous line!
            .expectNextSpeechUtteranceIsNot('Browser')
            .expectSpeech(/row [0-9]+ column 1/)

            .replay();
      });
    });

/**
 * Test fixture for DesktopAutomationHandler incl. TaskManagerDesktopRefresh.
 * `features::kTaskManagerDesktopRefresh` is included in the base fixture which
 * inherits from ChromeVoxE2ETest.
 */
ChromeVoxDesktopAutomationHandlerWithTaskManagerDesktopRefreshTest =
    class extends ChromeVoxDesktopAutomationHandlerTest {
  /** @override */
  get featureList() {
    let list = super.featureList || {};
    list.enabled = list.enabled || [];
    list.enabled.push('features::kTaskManagerDesktopRefresh');
    return list;
  }
};

TEST_F(
    'ChromeVoxDesktopAutomationHandlerWithTaskManagerDesktopRefreshTest',
    'TaskManagerTableView', function() {
      // TODO(crbug.com/397484647): Merge this test with the production
      // (ChromeVoxDesktopAutomationHandlerWithoutTaskManagerDesktopRefreshTest.
      // TaskManagerTableView) version after the flag is enabled by default.

      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedDesktop(desktop => {
        mockFeedback
            .call(() => {
              EventGenerator.sendKeyPress(KeyCode.ESCAPE, {search: true});
            })
            .expectSpeech('Task Manager, window')
            // The Task Manager Refresh has a TabbedPane UI which splits system
            // processes and user-facing processes (tabs & extensions). Expect
            // that the selected tab is focused when the window is open.
            .expectSpeech('Tabs & extensions, tab', ' 1 of 3 ', 'Selected')
            .call(() => {
              EventGenerator.sendKeyPress(KeyCode.TAB);
              EventGenerator.sendKeyPress(KeyCode.TAB);
              EventGenerator.sendKeyPress(KeyCode.DOWN);
            })
            // The Task Manager Refresh has a TabbedPane UI which splits system
            // processes and user-facing processes (tabs & extensions), so
            // expect that the first task is a Tab, not the Browser process.
            .expectSpeech('Tab: about:blank', /row [0-9]+ column 1/)
            .call(() => {
              EventGenerator.sendKeyPress(KeyCode.DOWN);
            })
            // Make sure it doesn't repeat the previous line!
            .expectNextSpeechUtteranceIsNot('Tab: about:blank')

            .replay();
      });
    });
