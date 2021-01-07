// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['select_to_speak_e2e_test_base.js']);
GEN_INCLUDE(['mock_tts.js']);

/**
 * Browser tests for select-to-speak's navigation control features.
 */
SelectToSpeakNavigationControlTest = class extends SelectToSpeakE2ETest {
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
      let module = await import('/select_to_speak/select_to_speak_main.js');
      window.selectToSpeak = module.selectToSpeak;

      module = await import('/select_to_speak/select_to_speak.js');
      window.SELECT_TO_SPEAK_TRAY_CLASS_NAME =
          module.SELECT_TO_SPEAK_TRAY_CLASS_NAME;

      module = await import('/select_to_speak/select_to_speak_constants.js');
      window.SelectToSpeakConstants = module.SelectToSpeakConstants;

      runTest();
    })();
  }

  /** @override */
  get featureList() {
    return {enabled: ['features::kSelectToSpeakNavigationControl']};
  }

  generateHtmlWithSelectedElement(elementId, bodyHtml) {
    return `
    <script type="text/javascript">
      function doSelection() {
        let selection = window.getSelection();
        let range = document.createRange();
        selection.removeAllRanges();
        let node = document.getElementById("${elementId}");
        range.selectNodeContents(node);
        selection.addRange(range);
      }
    </script>
    <body onload="doSelection()">${bodyHtml}</body>`;
  }
};

TEST_F(
    'SelectToSpeakNavigationControlTest', 'NavigatesToNextParagraph',
    function() {
      const bodyHtml = `
    <p id="p1">Paragraph 1</p>
    <p id="p2">Paragraph 2</p>'
  `;
      this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml), () => {
            this.triggerReadSelectedText();

            // Speaks first paragraph
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'Paragraph 1');

            // TODO(joelriley@google.com): Figure out a better way to trigger
            // the actual floating panel button rather than calling private
            // method directly.
            selectToSpeak.onSelectToSpeakPanelAction_(
                chrome.accessibilityPrivate.SelectToSpeakPanelAction
                    .NEXT_PARAGRAPH);

            // Speaks second paragraph
            this.waitOneEventLoop(() => {
              assertTrue(this.mockTts.currentlySpeaking());
              assertEquals(this.mockTts.pendingUtterances().length, 1);
              this.assertEqualsCollapseWhitespace(
                  this.mockTts.pendingUtterances()[0], 'Paragraph 2');
            });
          });
    });

TEST_F(
    'SelectToSpeakNavigationControlTest', 'NavigatesToPreviousParagraph',
    function() {
      const bodyHtml = `
    <p id="p1">Paragraph 1</p>
    <p id="p2">Paragraph 2</p>'
  `;
      this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p2', bodyHtml), () => {
            this.triggerReadSelectedText();

            // Speaks first paragraph
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'Paragraph 2');

            // TODO(joelriley@google.com): Figure out a better way to trigger
            // the actual floating panel button rather than calling private
            // method directly.
            selectToSpeak.onSelectToSpeakPanelAction_(
                chrome.accessibilityPrivate.SelectToSpeakPanelAction
                    .PREVIOUS_PARAGRAPH);

            // Speaks second paragraph
            this.waitOneEventLoop(() => {
              assertTrue(this.mockTts.currentlySpeaking());
              assertEquals(this.mockTts.pendingUtterances().length, 1);
              this.assertEqualsCollapseWhitespace(
                  this.mockTts.pendingUtterances()[0], 'Paragraph 1');
            });
          });
    });

TEST_F(
    'SelectToSpeakNavigationControlTest', 'ReadsParagraphOnClick', function() {
      const bodyHtml = `
      <p id="p1">Sentence <span>one</span>. Sentence two.</p>
      <p id="p2">Paragraph <span>two</span></p>'
    `;
      this.runWithLoadedTree(bodyHtml, (root) => {
        this.mockTts.setOnSpeechCallbacks([this.newCallback((utterance) => {
          // Speech for first click.
          assertTrue(this.mockTts.currentlySpeaking());
          assertEquals(this.mockTts.pendingUtterances().length, 1);
          this.assertEqualsCollapseWhitespace(
              this.mockTts.pendingUtterances()[0],
              'Sentence one . Sentence two.');

          this.mockTts.setOnSpeechCallbacks([this.newCallback((utterance) => {
            // Speech for second click.
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'Paragraph two');
          })]);

          // Click on node in second paragraph.
          const textNode2 = this.findTextNode(root, 'two');
          const mouseEvent2 = {
            screenX: textNode2.location.left + 1,
            screenY: textNode2.location.top + 1
          };
          this.triggerReadMouseSelectedText(mouseEvent2, mouseEvent2);
        })]);

        // Click on node in first paragraph.
        const textNode1 = this.findTextNode(root, 'one');
        const event1 = {
          screenX: textNode1.location.left + 1,
          screenY: textNode1.location.top + 1
        };
        this.triggerReadMouseSelectedText(event1, event1);
      });
    });

TEST_F(
    'SelectToSpeakNavigationControlTest', 'PauseResumeWithinTheSentence',
    function() {
      const bodyHtml = `
      <p id="p1">First sentence. Second sentence. Third sentence.</p>'
    `;
      this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml), () => {
            this.triggerReadSelectedText();

            // Speaks until the second word of the second sentence.
            this.mockTts.speakUntilCharIndex(23);
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0],
                'First sentence. Second sentence. Third sentence.');

            // Hitting pause will stop the current TTS.
            selectToSpeak.onSelectToSpeakPanelAction_(
                chrome.accessibilityPrivate.SelectToSpeakPanelAction.PAUSE);
            assertFalse(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 0);

            // Hitting resume will start from the beginning of the second
            // sentence.
            selectToSpeak.onSelectToSpeakPanelAction_(
                chrome.accessibilityPrivate.SelectToSpeakPanelAction.RESUME);
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0],
                'Second sentence. Third sentence.');
          });
    });

TEST_F(
    'SelectToSpeakNavigationControlTest', 'PauseResumeAtTheBeginningOfSentence',
    function() {
      const bodyHtml = `
      <p id="p1">First sentence. Second sentence. Third sentence.</p>'
    `;
      this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml), () => {
            this.triggerReadSelectedText();

            // Speaks until the third sentence.
            this.mockTts.speakUntilCharIndex(33);
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0],
                'First sentence. Second sentence. Third sentence.');

            // Hitting pause will stop the current TTS.
            selectToSpeak.onSelectToSpeakPanelAction_(
                chrome.accessibilityPrivate.SelectToSpeakPanelAction.PAUSE);
            assertFalse(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 0);

            // Hitting resume will start from the beginning of the third
            // sentence.
            selectToSpeak.onSelectToSpeakPanelAction_(
                chrome.accessibilityPrivate.SelectToSpeakPanelAction.RESUME);
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'Third sentence.');
          });
    });

TEST_F(
    'SelectToSpeakNavigationControlTest',
    'PauseResumeAtTheBeginningOfParagraph', function() {
      const bodyHtml = `
      <p id="p1">first sentence.</p>'
    `;
      this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml), () => {
            this.triggerReadSelectedText();

            // Speaks until the second word.
            this.mockTts.speakUntilCharIndex(6);
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'first sentence.');

            // Hitting pause will stop the current TTS.
            selectToSpeak.onSelectToSpeakPanelAction_(
                chrome.accessibilityPrivate.SelectToSpeakPanelAction.PAUSE);
            assertFalse(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 0);

            // Hitting resume will start from the beginning of the paragraph.
            selectToSpeak.onSelectToSpeakPanelAction_(
                chrome.accessibilityPrivate.SelectToSpeakPanelAction.RESUME);
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'first sentence.');
          });
    });

TEST_F('SelectToSpeakNavigationControlTest', 'NextSentence', function() {
  const bodyHtml = `
      <p id="p1">This is the first. This is the second.</p>'
    `;
  this.runWithLoadedTree(
      this.generateHtmlWithSelectedElement('p1', bodyHtml), async function() {
        this.triggerReadSelectedText();

        // Speaks the first word.
        this.mockTts.speakUntilCharIndex(5);
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0],
            'This is the first. This is the second.');

        // Hitting next sentence will start another TTS.
        await selectToSpeak.onSelectToSpeakPanelAction_(
            chrome.accessibilityPrivate.SelectToSpeakPanelAction.NEXT_SENTENCE);
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], 'This is the second.');
      });
});

TEST_F(
    'SelectToSpeakNavigationControlTest', 'NextSentenceWithinParagraph',
    function() {
      const bodyHtml = `
        <p id="p1">Sent 1. <span id="s1">Sent 2.</span> Sent 3. Sent 4.</p>
      `;
      this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('s1', bodyHtml), () => {
            this.triggerReadSelectedText();

            // Speaks the first word.
            this.mockTts.speakUntilCharIndex(5);
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'Sent 2.');

            // Hitting next sentence will start from the next sentence.
            selectToSpeak.onSelectToSpeakPanelAction_(
                chrome.accessibilityPrivate.SelectToSpeakPanelAction
                    .NEXT_SENTENCE);
            this.waitOneEventLoop(() => {
              assertTrue(this.mockTts.currentlySpeaking());
              assertEquals(this.mockTts.pendingUtterances().length, 1);
              this.assertEqualsCollapseWhitespace(
                  this.mockTts.pendingUtterances()[0], 'Sent 3. Sent 4.');
            });
          });
    });

TEST_F(
    'SelectToSpeakNavigationControlTest', 'NextSentenceAcrossParagraph',
    function() {
      const bodyHtml = `
        <p id="p1">Sent 1.</p>
        <p id="p2">Sent 2. Sent 3.</p>'
      `;
      this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml), () => {
            this.triggerReadSelectedText();

            // Speaks the first word.
            this.mockTts.speakUntilCharIndex(5);
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'Sent 1.');

            // Hitting next sentence will star from the next paragraph as there
            // is no more sentence in the current paragraph.
            selectToSpeak.onSelectToSpeakPanelAction_(
                chrome.accessibilityPrivate.SelectToSpeakPanelAction
                    .NEXT_SENTENCE);
            this.waitOneEventLoop(() => {
              assertTrue(this.mockTts.currentlySpeaking());
              assertEquals(this.mockTts.pendingUtterances().length, 1);
              this.assertEqualsCollapseWhitespace(
                  this.mockTts.pendingUtterances()[0], 'Sent 2. Sent 3.');
            });
          });
    });

TEST_F('SelectToSpeakNavigationControlTest', 'PrevSentence', function() {
  const bodyHtml = `
      <p id="p1">First sentence. Second sentence. Third sentence.</p>'
    `;
  this.runWithLoadedTree(
      this.generateHtmlWithSelectedElement('p1', bodyHtml), async function() {
        this.triggerReadSelectedText();

        // Speaks util the start of the second sentence.
        this.mockTts.speakUntilCharIndex(33);
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0],
            'First sentence. Second sentence. Third sentence.');

        // Hitting prev sentence will start another TTS.
        await selectToSpeak.onSelectToSpeakPanelAction_(
            chrome.accessibilityPrivate.SelectToSpeakPanelAction
                .PREVIOUS_SENTENCE);
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0],
            'Second sentence. Third sentence.');
      });
});

TEST_F(
    'SelectToSpeakNavigationControlTest', 'PrevSentenceFromMiddleOfSentence',
    function() {
      const bodyHtml = `
      <p id="p1">First sentence. Second sentence. Third sentence.</p>'
    `;
      this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml),
          async function() {
            this.triggerReadSelectedText();

            // Speaks util the start of "sentence" in "Second sentence".
            this.mockTts.speakUntilCharIndex(23);
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0],
                'First sentence. Second sentence. Third sentence.');

            // Hitting prev sentence will start another TTS.
            await selectToSpeak.onSelectToSpeakPanelAction_(
                chrome.accessibilityPrivate.SelectToSpeakPanelAction
                    .PREVIOUS_SENTENCE);
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0],
                'First sentence. Second sentence. Third sentence.');
          });
    });

TEST_F(
    'SelectToSpeakNavigationControlTest', 'PrevSentenceWithinParagraph',
    function() {
      const bodyHtml = `
      <p id="p1">Sent 0. Sent 1. <span id="s1">Sent 2.</span> Sent 3.</p>
    `;
      this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('s1', bodyHtml), () => {
            this.triggerReadSelectedText();

            // Supposing we are at the start of the sentence.
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'Sent 2.');

            // Hitting previous sentence will start from the previous sentence.
            selectToSpeak.onSelectToSpeakPanelAction_(
                chrome.accessibilityPrivate.SelectToSpeakPanelAction
                    .PREVIOUS_SENTENCE);
            this.waitOneEventLoop(() => {
              assertTrue(this.mockTts.currentlySpeaking());
              assertEquals(this.mockTts.pendingUtterances().length, 1);
              this.assertEqualsCollapseWhitespace(
                  this.mockTts.pendingUtterances()[0],
                  'Sent 1. Sent 2. Sent 3.');
            });
          });
    });

TEST_F(
    'SelectToSpeakNavigationControlTest', 'PrevSentenceAcrossParagraph',
    function() {
      const bodyHtml = `
      <p id="p1">Sent 1. Sent 2.</p>
      <p id="p2">Sent 3.</p>'
    `;
      this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p2', bodyHtml), () => {
            this.triggerReadSelectedText();

            // We are at the start of the sentence.
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'Sent 3.');

            // Hitting previous sentence will start from the last sentence in
            // the previous paragraph as there is no more sentence in the
            // current paragraph.
            selectToSpeak.onSelectToSpeakPanelAction_(
                chrome.accessibilityPrivate.SelectToSpeakPanelAction
                    .PREVIOUS_SENTENCE);
            this.waitOneEventLoop(() => {
              assertTrue(this.mockTts.currentlySpeaking());
              assertEquals(this.mockTts.pendingUtterances().length, 1);
              this.assertEqualsCollapseWhitespace(
                  this.mockTts.pendingUtterances()[0], 'Sent 2.');
            });
          });
    });

// TODO(https://crbug.com/1157817): Fix Flaky Test.
TEST_F(
    'SelectToSpeakNavigationControlTest', 'ChangeSpeedWhilePlaying',
    function() {
      chrome.settingsPrivate.setPref('settings.tts.speech_rate', 1.2);
      const bodyHtml = `
      <p id="p1">Paragraph 1</p>'
    `;
      this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml), () => {
            this.triggerReadSelectedText();

            // Speaks the first word.
            this.mockTts.speakUntilCharIndex(10);
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'Paragraph 1');
            assertEquals(this.mockTts.getOptions().rate, 1.2);

            // Changing speed will resume at the start of the current sentence.
            selectToSpeak.onSelectToSpeakPanelAction_(
                chrome.accessibilityPrivate.SelectToSpeakPanelAction
                    .CHANGE_SPEED,
                1.5);
            assertFalse(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 0);

            // Wait an event loop so all pending promises are resolved prior to
            // asserting that TTS resumed with the proper rate.
            setTimeout(
                this.newCallback(() => {
                  // Should resume TTS at the sentence boundary.
                  assertTrue(this.mockTts.currentlySpeaking());
                  assertEquals(this.mockTts.getOptions().rate, 1.5);
                  assertEquals(this.mockTts.pendingUtterances().length, 1);
                  this.assertEqualsCollapseWhitespace(
                      this.mockTts.pendingUtterances()[0], 'Paragraph 1');
                }),
                0);
          });
    });

TEST_F(
    'SelectToSpeakNavigationControlTest', 'ChangeSpeedWhilePaused', function() {
      chrome.settingsPrivate.setPref('settings.tts.speech_rate', 1.2);
      const bodyHtml = `
      <p id="p1">Paragraph 1</p>'
    `;
      this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml), () => {
            this.triggerReadSelectedText();

            // Speaks the first word.
            this.mockTts.speakUntilCharIndex(10);
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'Paragraph 1');
            assertEquals(this.mockTts.getOptions().rate, 1.2);

            // User-intiated pause.
            selectToSpeak.onSelectToSpeakPanelAction_(
                chrome.accessibilityPrivate.SelectToSpeakPanelAction.PAUSE);
            assertFalse(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 0);

            // Changing speed will remain paused.
            selectToSpeak.onSelectToSpeakPanelAction_(
                chrome.accessibilityPrivate.SelectToSpeakPanelAction
                    .CHANGE_SPEED,
                1.5);

            // Wait an event loop so all pending promises are resolved prior to
            // asserting that TTS remains paused.
            setTimeout(this.newCallback(() => {
              assertFalse(this.mockTts.currentlySpeaking());
              assertEquals(this.mockTts.pendingUtterances().length, 0);
            }, 0));
          });
    });

TEST_F(
    'SelectToSpeakNavigationControlTest', 'ResumeAtTheEndOfParagraph',
    function() {
      const bodyHtml = `
        <p id="p1">Paragraph 1</p>
        <p id="p2">Paragraph 2</p>
      `;
      this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml), () => {
            this.triggerReadSelectedText();

            // Finishes the current utterance.
            this.mockTts.finishPendingUtterance();

            // Hitting resume will start the next paragraph.
            selectToSpeak.onSelectToSpeakPanelAction_(
                chrome.accessibilityPrivate.SelectToSpeakPanelAction.RESUME);
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'Paragraph 2');
          });
    });

TEST_F(
    'SelectToSpeakNavigationControlTest', 'ResumeAtTheEndOfUserSelection',
    function() {
      const bodyHtml = `
        <p id="p1">Sentence <span id="s1">one</span>. Sentence two.</p>
        <p id="p2">Paragraph 2</p>
      `;
      this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('s1', bodyHtml), () => {
            this.triggerReadSelectedText();

            // Finishes the current utterance.
            this.mockTts.finishPendingUtterance();

            // Hitting resume will start the remaining content.
            selectToSpeak.onSelectToSpeakPanelAction_(
                chrome.accessibilityPrivate.SelectToSpeakPanelAction.RESUME);
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], '. Sentence two.');
          });
    });

TEST_F('SelectToSpeakNavigationControlTest', 'ResizeWhilePlaying', function() {
  const longLine =
      'Second paragraph is longer than 300 pixels and will wrap when resized';
  const bodyHtml = `
          <script type="text/javascript">
            function doResize() {
              document.getElementById('resize').style.width = '100px';
            }
          </script>
          <div id="content">
            <p>First paragraph</p>
            <p id='resize' style='width:300px; font-size: 1em'>
              ${longLine}
            </p>
          </div>
          <button onclick="doResize()">Resize</button>
        `;
  this.runWithLoadedTree(
      this.generateHtmlWithSelectedElement('content', bodyHtml), (root) => {
        this.triggerReadSelectedText();

        // Speaks the first paragraph.
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], 'First paragraph');

        const resizeButton =
            root.find({role: 'button', attributes: {name: 'Resize'}});

        // Wait for click event, at which point the automation tree should
        // be updated from the resize.
        resizeButton.addEventListener(
            EventType.CLICKED, this.newCallback(() => {
              // Trigger next node group by completing first TTS request.
              this.mockTts.finishPendingUtterance();

              // Should still read second paragraph, even though some nodes
              // were invalided from the resize.
              assertTrue(this.mockTts.currentlySpeaking());
              assertEquals(this.mockTts.pendingUtterances().length, 1);
              this.assertEqualsCollapseWhitespace(
                  this.mockTts.pendingUtterances()[0], longLine);
            }));

        // Perform resize.
        resizeButton.doDefault();
      });
});
