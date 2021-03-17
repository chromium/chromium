// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['select_to_speak_e2e_test_base.js']);
GEN_INCLUDE(['mock_tts.js']);

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
  setUp() {
    var runTest = this.deferRunTest(WhenTestDone.EXPECT);

    window.EventType = chrome.automation.EventType;
    window.SelectToSpeakState = chrome.accessibilityPrivate.SelectToSpeakState;

    (async function() {
      await importModule(
          'selectToSpeak', '/select_to_speak/select_to_speak_main.js');
      await importModule(
          'SELECT_TO_SPEAK_TRAY_CLASS_NAME', '/select_to_speak/ui_manager.js');
      await importModule(
          'SelectToSpeakConstants',
          '/select_to_speak/select_to_speak_constants.js');
      runTest();
    })();
  }

  tapTrayButton(desktop, callback) {
    const button = desktop.find({
      attributes: {className: SELECT_TO_SPEAK_TRAY_CLASS_NAME}
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

TEST_F('SelectToSpeakMouseSelectionTest', 'SpeaksNodeWhenClicked', function() {
  this.runWithLoadedTree(
      'data:text/html;charset=utf-8,' +
          '<p>This is some text</p>',
      function(root) {
        assertFalse(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 0);
        this.mockTts.setOnSpeechCallbacks(
            [this.newCallback(function(utterance) {
              // Speech starts asynchronously.
              assertTrue(this.mockTts.currentlySpeaking());
              assertEquals(this.mockTts.pendingUtterances().length, 1);
              this.assertEqualsCollapseWhitespace(
                  this.mockTts.pendingUtterances()[0], 'This is some text');
            })]);
        const textNode = this.findTextNode(root, 'This is some text');
        const event = {
          screenX: textNode.location.left + 1,
          screenY: textNode.location.top + 1
        };
        this.triggerReadMouseSelectedText(event, event);
      });
});

TEST_F(
    'SelectToSpeakMouseSelectionTest', 'SpeaksMultipleNodesWhenDragged',
    function() {
      this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
              '<p>This is some text</p><p>This is some more text</p>',
          function(root) {
            assertFalse(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 0);
            this.mockTts.setOnSpeechCallbacks([
              this.newCallback(function(utterance) {
                assertTrue(this.mockTts.currentlySpeaking());
                assertEquals(this.mockTts.pendingUtterances().length, 1);
                this.assertEqualsCollapseWhitespace(
                    utterance, 'This is some text');
                this.mockTts.finishPendingUtterance();
              }),
              this.newCallback(function(utterance) {
                this.assertEqualsCollapseWhitespace(
                    utterance, 'This is some more text');
              })
            ]);
            const firstNode = this.findTextNode(root, 'This is some text');
            const downEvent = {
              screenX: firstNode.location.left + 1,
              screenY: firstNode.location.top + 1
            };
            const lastNode = this.findTextNode(root, 'This is some more text');
            const upEvent = {
              screenX: lastNode.location.left + lastNode.location.width,
              screenY: lastNode.location.top + lastNode.location.height
            };
            this.triggerReadMouseSelectedText(downEvent, upEvent);
          });
    });

TEST_F(
    'SelectToSpeakMouseSelectionTest', 'SpeaksAcrossNodesInAParagraph',
    function() {
      this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
              '<p style="width:200px">This is some text in a paragraph that wraps. ' +
              '<i>Italic text</i></p>',
          function(root) {
            assertFalse(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 0);
            this.mockTts.setOnSpeechCallbacks(
                [this.newCallback(function(utterance) {
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
              screenY: firstNode.location.top + 1
            };
            const lastNode = this.findTextNode(root, 'Italic text');
            const upEvent = {
              screenX: lastNode.location.left + lastNode.location.width,
              screenY: lastNode.location.top + lastNode.location.height
            };
            this.triggerReadMouseSelectedText(downEvent, upEvent);
          });
    });

TEST_F(
    'SelectToSpeakMouseSelectionTest', 'SpeaksNodeAfterTrayTapAndMouseClick',
    function() {
      this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
              '<p>This is some text</p>',
          function(root) {
            assertFalse(this.mockTts.currentlySpeaking());
            this.mockTts.setOnSpeechCallbacks(
                [this.newCallback(function(utterance) {
                  // Speech starts asynchronously.
                  assertTrue(this.mockTts.currentlySpeaking());
                  assertEquals(this.mockTts.pendingUtterances().length, 1);
                  this.assertEqualsCollapseWhitespace(
                      this.mockTts.pendingUtterances()[0], 'This is some text');
                })]);

            const textNode = this.findTextNode(root, 'This is some text');
            const event = {
              screenX: textNode.location.left + 1,
              screenY: textNode.location.top + 1
            };
            // A state change request should shift us into 'selecting' state
            // from 'inactive'.
            const desktop = root.parent.root;
            this.tapTrayButton(desktop, () => {
              selectToSpeak.fireMockMouseDownEvent(event);
              selectToSpeak.fireMockMouseUpEvent(event);
            });
          });
    });

TEST_F(
    'SelectToSpeakMouseSelectionTest', 'CancelsSelectionModeWithStateChange',
    function() {
      this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
              '<p>This is some text</p>',
          function(root) {
            const textNode = this.findTextNode(root, 'This is some text');
            const event = {
              screenX: textNode.location.left + 1,
              screenY: textNode.location.top + 1
            };
            // A state change request should shift us into 'selecting' state
            // from 'inactive'.
            const desktop = root.parent.root;
            this.tapTrayButton(desktop, () => {
              selectToSpeak.fireMockMouseDownEvent(event);
              assertEquals(SelectToSpeakState.SELECTING, selectToSpeak.state_);

              // Another state change puts us back in 'inactive'.
              this.tapTrayButton(desktop, () => {
                assertEquals(SelectToSpeakState.INACTIVE, selectToSpeak.state_);
              });
            });
          });
    });

TEST_F(
    'SelectToSpeakMouseSelectionTest', 'CancelsSpeechWithTrayTap', function() {
      this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
              '<p>This is some text</p>',
          function(root) {
            assertFalse(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 0);
            this.mockTts.setOnSpeechCallbacks(
                [this.newCallback(function(utterance) {
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
                    assertEquals(
                        SelectToSpeakState.INACTIVE, selectToSpeak.state_);
                  });
                })]);
            const textNode = this.findTextNode(root, 'This is some text');
            const event = {
              screenX: textNode.location.left + 1,
              screenY: textNode.location.top + 1
            };
            this.triggerReadMouseSelectedText(event, event);
          });
    });

// TODO(crbug.com/1177140) Re-enable test
TEST_F(
    'SelectToSpeakMouseSelectionTest', 'DISABLED_DoesNotSpeakOnlyTheTrayButton',
    function() {
      // The tray button itself should not be spoken when clicked in selection
      // mode per UI review (but if more elements are being verbalized than just
      // the STS tray button, it may be spoken). This is similar to how the
      // stylus may act as a laser pointer unless it taps on the stylus options
      // button, which always opens on a tap regardless of the stylus behavior
      // selected.
      this.runWithLoadedDesktop((desktop) => {
        this.tapTrayButton(desktop, () => {
          assertEquals(selectToSpeak.state_, SelectToSpeakState.SELECTING);
          const button = desktop.find({
            attributes: {className: SELECT_TO_SPEAK_TRAY_CLASS_NAME}
          });

          // Use the same automation callbacks as Select-to-Speak to make
          // sure we actually don't start speech after the hittest and focus
          // callbacks are used to check which nodes should be spoken.
          desktop.addEventListener(
              EventType.MOUSE_RELEASED, this.newCallback((evt) => {
                chrome.automation.getFocus(this.newCallback((node) => {
                  assertEquals(
                      selectToSpeak.state_, SelectToSpeakState.INACTIVE);
                  assertFalse(this.mockTts.currentlySpeaking());
                  assertEquals(this.mockTts.pendingUtterances().length, 0);
                }));
              }),
              true);

          const event = {
            screenX: button.location.left + 1,
            screenY: button.location.top + 1
          };
          selectToSpeak.fireMockMouseDownEvent(event);
          selectToSpeak.fireMockMouseUpEvent(event);
        });
      });
    });
