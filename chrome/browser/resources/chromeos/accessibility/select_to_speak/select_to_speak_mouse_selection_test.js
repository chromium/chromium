// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['select_to_speak_e2e_test_base.js']);
GEN_INCLUDE(['../common/testing/mock_tts.js']);

/**
 * Browser tests for select-to-speak's feature to speak text
 * by holding down a key and clicking or dragging with the mouse.
 */
SelectToSpeakMouseSelectionTest = class extends SelectToSpeakE2ETest {
  constructor() {
    super();
    this.mockTts = new MockTts();
    chrome.tts = this.mockTts;
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    window.EventType = chrome.automation.EventType;
    window.SelectToSpeakState = chrome.accessibilityPrivate.SelectToSpeakState;

    await new Promise(resolve => {
      chrome.settingsPrivate.setPref(
          PrefsManager.ENHANCED_VOICES_DIALOG_SHOWN_KEY, true,
          '' /* unused, see crbug.com/866161 */, () => resolve());
    });
    if (!selectToSpeak.prefsManager_.enhancedVoicesDialogShown()) {
      // TODO(b/267705784): This shouldn't happen, but sometimes the
      // setPref call above does not cause PrefsManager.updateSettingsPrefs_ to
      // be called (test: listen to updateSettingsPrefsCallbackForTest_, never
      // called).
      selectToSpeak.prefsManager_.enhancedVoicesDialogShown_ = true;
    }
  }

  tapTrayButton(desktop, callback) {
    const button = desktop.find({
      attributes: {className: SELECT_TO_SPEAK_TRAY_CLASS_NAME},
    });

    callback = this.newCallback(callback);
    selectToSpeak.onStateChangeRequestedCallbackForTest_ =
        this.newCallback(() => {
          selectToSpeak.onStateChangeRequestedCallbackForTest_ = null;
          callback();
        });
    button.doDefault();
  }
};

AX_TEST_F(
    'SelectToSpeakMouseSelectionTest', 'SpeaksNodeWhenClicked',
    async function() {
      const root = await this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
          '<p>This is some text</p>');
      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);
      this.mockTts.setOnSpeechCallbacks([this.newCallback(function(utterance) {
        // Speech starts asynchronously.
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], 'This is some text');
      })]);
      const textNode = this.findTextNode(root, 'This is some text');
      const event = {
        screenX: textNode.location.left + 1,
        screenY: textNode.location.top + 1,
      };
      this.triggerReadMouseSelectedText(event, event);
    });

AX_TEST_F(
    'SelectToSpeakMouseSelectionTest', 'SpeaksMultipleNodesWhenDragged',
    async function() {
      const root = await this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
          '<p>This is some text</p><p>This is some more text</p>');
      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);
      this.mockTts.setOnSpeechCallbacks([
        this.newCallback(function(utterance) {
          assertTrue(this.mockTts.currentlySpeaking());
          assertEquals(this.mockTts.pendingUtterances().length, 1);
          this.assertEqualsCollapseWhitespace(utterance, 'This is some text');
          this.mockTts.finishPendingUtterance();
        }),
        this.newCallback(function(utterance) {
          this.assertEqualsCollapseWhitespace(
              utterance, 'This is some more text');
        }),
      ]);
      const firstNode = this.findTextNode(root, 'This is some text');
      const downEvent = {
        screenX: firstNode.location.left + 1,
        screenY: firstNode.location.top + 1,
      };
      const lastNode = this.findTextNode(root, 'This is some more text');
      const upEvent = {
        screenX: lastNode.location.left + lastNode.location.width,
        screenY: lastNode.location.top + lastNode.location.height,
      };
      this.triggerReadMouseSelectedText(downEvent, upEvent);
    });

AX_TEST_F(
    'SelectToSpeakMouseSelectionTest', 'SpeaksAcrossNodesInAParagraph',
    async function() {
      const root = await this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
          '<p style="width:200px">This is some text in a paragraph that ' +
          'wraps. <i>Italic text</i></p>');
      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);
      this.mockTts.setOnSpeechCallbacks([this.newCallback(function(utterance) {
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            utterance,
            'This is some text in a paragraph that wraps. ' +
                'Italic text');
      })]);
      const firstNode = this.findTextNode(
          root, 'This is some text in a paragraph that wraps. ');
      const downEvent = {
        screenX: firstNode.location.left + 1,
        screenY: firstNode.location.top + 1,
      };
      const lastNode = this.findTextNode(root, 'Italic text');
      const upEvent = {
        screenX: lastNode.location.left + lastNode.location.width,
        screenY: lastNode.location.top + lastNode.location.height,
      };
      this.triggerReadMouseSelectedText(downEvent, upEvent);
    });

AX_TEST_F(
    'SelectToSpeakMouseSelectionTest', 'SpeaksNodeAfterTrayTapAndMouseClick',
    async function() {
      const root = await this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
          '<p>This is some text</p>');
      assertFalse(this.mockTts.currentlySpeaking());
      this.mockTts.setOnSpeechCallbacks([this.newCallback(function(utterance) {
        // Speech starts asynchronously.
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], 'This is some text');
      })]);

      const textNode = this.findTextNode(root, 'This is some text');
      const mouseX = textNode.location.left + 1;
      const mouseY = textNode.location.top + 1;
      // A state change request should shift us into 'selecting' state
      // from 'inactive'.
      const desktop = root.parent.root;
      this.tapTrayButton(desktop, () => {
        selectToSpeak.fireMockMouseEvent(
            chrome.accessibilityPrivate.SyntheticMouseEventType.PRESS, mouseX,
            mouseY);
        selectToSpeak.fireMockMouseEvent(
            chrome.accessibilityPrivate.SyntheticMouseEventType.RELEASE, mouseX,
            mouseY);
      });
    });

AX_TEST_F(
    'SelectToSpeakMouseSelectionTest', 'CancelsSelectionModeWithStateChange',
    async function() {
      const root = await this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
          '<p>This is some text</p>');
      const textNode = this.findTextNode(root, 'This is some text');
      // A state change request should shift us into 'selecting' state
      // from 'inactive'.
      const desktop = root.parent.root;
      this.tapTrayButton(desktop, () => {
        selectToSpeak.fireMockMouseEvent(
            chrome.accessibilityPrivate.SyntheticMouseEventType.PRESS,
            textNode.location.left + 1, textNode.location.top + 1);
        assertEquals(SelectToSpeakState.SELECTING, selectToSpeak.state_);

        // Another state change puts us back in 'inactive'.
        this.tapTrayButton(desktop, () => {
          assertEquals(SelectToSpeakState.INACTIVE, selectToSpeak.state_);
        });
      });
    });

AX_TEST_F(
    'SelectToSpeakMouseSelectionTest', 'CancelsSpeechWithTrayTap',
    async function() {
      const root = await this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
          '<p>This is some text</p>');
      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);
      this.mockTts.setOnSpeechCallbacks([this.newCallback(function(utterance) {
        // Speech starts asynchronously.
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], 'This is some text');

        // Cancel speech and make sure state resets to INACTIVE.
        const desktop = root.parent.root;
        this.tapTrayButton(desktop, () => {
          assertFalse(this.mockTts.currentlySpeaking());
          assertEquals(this.mockTts.pendingUtterances().length, 0);
          assertEquals(SelectToSpeakState.INACTIVE, selectToSpeak.state_);
        });
      })]);
      const textNode = this.findTextNode(root, 'This is some text');
      const event = {
        screenX: textNode.location.left + 1,
        screenY: textNode.location.top + 1,
      };
      this.triggerReadMouseSelectedText(event, event);
    });

// TODO(crbug.com/40748296) Re-enable test
TEST_F(
    'SelectToSpeakMouseSelectionTest', 'DISABLED_DoesNotSpeakOnlyTheTrayButton',
    function() {
      // The tray button itself should not be spoken when clicked in selection
      // mode per UI review (but if more elements are being verbalized than just
      // the STS tray button, it may be spoken). This is similar to how the
      // stylus may act as a laser pointer unless it taps on the stylus options
      // button, which always opens on a tap regardless of the stylus behavior
      // selected.
      this.runWithLoadedDesktop(desktop => {
        this.tapTrayButton(desktop, () => {
          assertEquals(selectToSpeak.state_, SelectToSpeakState.SELECTING);
          const button = desktop.find({
            attributes: {className: SELECT_TO_SPEAK_TRAY_CLASS_NAME},
          });

          // Use the same automation callbacks as Select-to-Speak to make
          // sure we actually don't start speech after the hittest and focus
          // callbacks are used to check which nodes should be spoken.
          desktop.addEventListener(
              EventType.MOUSE_RELEASED, this.newCallback(evt => {
                chrome.automation.getFocus(this.newCallback(node => {
                  assertEquals(
                      selectToSpeak.state_, SelectToSpeakState.INACTIVE);
                  assertFalse(this.mockTts.currentlySpeaking());
                  assertEquals(this.mockTts.pendingUtterances().length, 0);
                }));
              }),
              true);

          const mouseX = button.location.left + 1;
          const mouseY = button.location.top + 1;
          selectToSpeak.fireMockMouseEvent(
              chrome.accessibilityPrivate.SyntheticMouseEventType.PRESS, mouseX,
              mouseY);
          selectToSpeak.fireMockMouseEvent(
              chrome.accessibilityPrivate.SyntheticMouseEventType.RELEASE,
              mouseX, mouseY);
        });
      });
    });

AX_TEST_F(
    'SelectToSpeakMouseSelectionTest', 'VoiceSwitching', async function() {
      selectToSpeak.shouldUseVoiceSwitching_ = () => true;
      const root = await this.runWithLoadedTree(
          'data:text/html;charset=utf-8,<div>' +
          '<span lang="fr-FR">The first paragraph</span>' +
          '<span lang="en-US">The second paragraph</span></div>');

      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);
      this.mockTts.setOnSpeechCallbacks([
        this.newCallback(function(utterance) {
          const options = this.mockTts.getOptions();
          assertEquals('fr-FR', options.lang);
          assertEquals(undefined, options.voiceName);
          this.assertEqualsCollapseWhitespace(
              this.mockTts.pendingUtterances()[0], 'The first paragraph');
          this.mockTts.finishPendingUtterance();
        }),
        this.newCallback(function(utterance) {
          const options = this.mockTts.getOptions();
          assertEquals('en-US', options.lang);
          assertEquals(undefined, options.voiceName);
          this.assertEqualsCollapseWhitespace(
              this.mockTts.pendingUtterances()[0], 'The second paragraph');
        }),
      ]);

      const firstNode = this.findTextNode(root, 'The first paragraph');
      assertNotNullNorUndefined(firstNode);
      const downEvent = {
        screenX: firstNode.location.left + 1,
        screenY: firstNode.location.top + 1,
      };
      const lastNode = this.findTextNode(root, 'The second paragraph');
      assertNotNullNorUndefined(lastNode);
      const upEvent = {
        screenX: lastNode.location.left + lastNode.location.width,
        screenY: lastNode.location.top + lastNode.location.height,
      };
      this.triggerReadMouseSelectedText(downEvent, upEvent);
    });

AX_TEST_F('SelectToSpeakMouseSelectionTest', 'SystemUI', async function() {
  this.runWithLoadedDesktop(async desktop => {
    // Select STS tray and system tray to ensure STS tray is spoken.
    // We can test against the STS tray text because we own it, the
    // rest of the system tray may change.
    const systemTray = desktop.find({
      attributes: {className: 'UnifiedSystemTray'},
    });
    const stsTray = desktop.find({
      attributes: {className: SELECT_TO_SPEAK_TRAY_CLASS_NAME},
    });
    const start = {
      screenX: stsTray.location.left + 1,
      screenY: stsTray.location.top + 1,
    };
    const end = {
      screenX: systemTray.location.left + systemTray.location.width - 1,
      screenY: systemTray.location.top + 10,
    };
    this.mockTts.setOnSpeechCallbacks([this.newCallback(function(utterance) {
      assertTrue(this.mockTts.currentlySpeaking());
      // Sometimes we get "Select-to-speak button" and sometimes
      // "Select-to-speak" and sometimes "Highlight text on your screen". Any
      // are acceptable.
      const trimmedUtterance =
          utterance.replace(/button/, '').toLowerCase().trim();
      assertTrue(['select-to-speak', 'highlight text on your screen'].includes(
          trimmedUtterance));
    })]);

    focusRingsCallback = this.newCallback((focusRings) => {
      // Check focus rings are reasonably sized.
      assertTrue(focusRings[0].rects[0].width < 200);
      assertTrue(focusRings[0].rects[0].height < 100);
    });
    // Override focus rings method for this test.
    chrome.accessibilityPrivate.setFocusRings = rings => {
      if (focusRingsCallback && rings.length > 0 && rings[0].rects.length > 0) {
        focusRingsCallback(rings);
        focusRingsCallback = null;
      }
    };

    this.triggerReadMouseSelectedText(start, end);
  });
});
