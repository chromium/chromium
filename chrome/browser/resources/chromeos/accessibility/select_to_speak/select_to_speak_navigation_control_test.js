// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['select_to_speak_e2e_test_base.js']);
GEN_INCLUDE(['../common/testing/mock_tts.js']);

/**
 * Browser tests for select-to-speak's navigation control features.
 */
SelectToSpeakNavigationControlTest = class extends SelectToSpeakE2ETest {
  constructor() {
    super();
    this.mockTts = new MockTts();
    chrome.tts = this.mockTts;

    // Save original updateSelectToSpeakPanel function so we can override it in
    // tests, then later restore the original implementation.
    this.updateSelectToSpeakPanel =
        chrome.accessibilityPrivate.updateSelectToSpeakPanel;
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    window.EventType = chrome.automation.EventType;
    window.RoleType = chrome.automation.RoleType;
    window.SelectToSpeakState = chrome.accessibilityPrivate.SelectToSpeakState;
    chrome.accessibilityPrivate.updateSelectToSpeakPanel =
        this.updateSelectToSpeakPanel;

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

  isNodeWithinPanel(node) {
    const windowParent =
        AutomationUtil.getFirstAncestorWithRole(node, RoleType.WINDOW);
    return windowParent.className === 'TrayBubbleView' &&
        windowParent.children.length === 1 &&
        windowParent.children[0].className === 'SelectToSpeakMenuView';
  }

  waitForPanelFocus(root, callback) {
    callback = this.newCallback(callback);
    const focusCallback = () => {
      chrome.automation.getFocus(node => {
        if (!this.isNodeWithinPanel(node)) {
          return;
        }
        root.removeEventListener(EventType.FOCUS, focusCallback);
        callback(node);
      });
    };
    root.addEventListener(EventType.FOCUS, focusCallback);
  }
};

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'NavigatesToNextParagraph',
    async function() {
      const bodyHtml = `
    <p id="p1">Paragraph 1</p>
    <p id="p2">Paragraph 2</p>'
  `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml));
      this.triggerReadSelectedText();

      // Speaks first paragraph
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Paragraph 1');

      // TODO(joelriley@google.com): Figure out a better way to trigger
      // the actual floating panel button rather than calling private
      // method directly.
      selectToSpeak.onNextParagraphRequested();

      // Speaks second paragraph
      this.waitOneEventLoop(() => {
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], 'Paragraph 2');
      });
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'NavigatesToPreviousParagraph',
    async function() {
      const bodyHtml = `
    <p id="p1">Paragraph 1</p>
    <p id="p2">Paragraph 2</p>'
  `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p2', bodyHtml));
      this.triggerReadSelectedText();

      // Speaks first paragraph
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Paragraph 2');

      // TODO(joelriley@google.com): Figure out a better way to trigger
      // the actual floating panel button rather than calling private
      // method directly.
      selectToSpeak.onPreviousParagraphRequested();

      // Speaks second paragraph
      this.waitOneEventLoop(() => {
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], 'Paragraph 1');
      });
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'ReadsParagraphOnClick',
    async function() {
      const bodyHtml = `
      <p id="p1">Sentence <span>one</span>. Sentence two.</p>
      <p id="p2">Paragraph <span>two</span></p>'
    `;
      const root = await this.runWithLoadedTree(bodyHtml);
      this.mockTts.setOnSpeechCallbacks([
        this.newCallback(utterance => {
          // Speech for first click.
          assertTrue(this.mockTts.currentlySpeaking());
          assertEquals(this.mockTts.pendingUtterances().length, 1);
          this.assertEqualsCollapseWhitespace(
              this.mockTts.pendingUtterances()[0],
              'Sentence one . Sentence two.');

          this.mockTts.setOnSpeechCallbacks([this.newCallback(utterance => {
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
            screenY: textNode2.location.top + 1,
          };
          this.triggerReadMouseSelectedText(mouseEvent2, mouseEvent2);
        }),
      ]);

      // Click on node in first paragraph.
      const textNode1 = this.findTextNode(root, 'one');
      const event1 = {
        screenX: textNode1.location.left + 1,
        screenY: textNode1.location.top + 1,
      };
      this.triggerReadMouseSelectedText(event1, event1);
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'PauseResumeWithinTheSentence',
    async function() {
      const bodyHtml = `
      <p id="p1">First sentence. Second sentence. Third sentence.</p>'
    `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml));
      this.triggerReadSelectedText();

      // Speaks until the second word of the second sentence.
      this.mockTts.speakUntilCharIndex(23);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0],
          'First sentence. Second sentence. Third sentence.');

      // Hitting pause will stop the current TTS.
      selectToSpeak.onPauseRequested();
      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);

      // Hitting resume will start from the remaining content of the
      // second sentence.
      selectToSpeak.onResumeRequested();
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'sentence. Third sentence.');
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'PauseResumeAtTheBeginningOfSentence',
    async function() {
      const bodyHtml = `
      <p id="p1">First sentence. Second sentence. Third sentence.</p>'
    `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml));
      this.triggerReadSelectedText();

      // Speaks until the third sentence.
      this.mockTts.speakUntilCharIndex(33);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0],
          'First sentence. Second sentence. Third sentence.');

      // Hitting pause will stop the current TTS.
      selectToSpeak.onPauseRequested();
      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);

      // Hitting resume will start from the beginning of the third
      // sentence.
      selectToSpeak.onResumeRequested();
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Third sentence.');
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest',
    'PauseResumeAtTheBeginningOfParagraph', async function() {
      const bodyHtml = `
      <p id="p1">first sentence.</p>'
    `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml));
      this.triggerReadSelectedText();

      // Speaks until the second word.
      this.mockTts.speakUntilCharIndex(6);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'first sentence.');

      // Hitting pause will stop the current TTS.
      selectToSpeak.onPauseRequested();
      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);

      // Hitting resume will start from the remaining content of the
      // paragraph.
      selectToSpeak.onResumeRequested();
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'sentence.');
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest',
    'PauseResumeInTheMiddleOfMultiParagraphs', async function() {
      const bodyHtml = `
      <span id='s1'>
        <p>Paragraph one.</p>
        <p>Paragraph two.</p>
        <p>Paragraph three.</p>
      </span>'
      `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('s1', bodyHtml));
      this.triggerReadSelectedText();

      // Speaks until the second word.
      this.mockTts.speakUntilCharIndex(10);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Paragraph one.');

      // Hitting pause will stop the current TTS.
      selectToSpeak.onPauseRequested();
      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);

      // Hitting resume will start from the remaining content of the
      // paragraph.
      selectToSpeak.onResumeRequested();
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'one.');

      // Keep reading will finish all the content.
      this.mockTts.finishPendingUtterance();
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Paragraph two.');
      this.mockTts.finishPendingUtterance();
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Paragraph three.');
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'PauseResumeAfterParagraphNavigation',
    async function() {
      const bodyHtml = `
      <span id='s1'>
        <p>Paragraph one.</p>
        <p>Paragraph two.</p>
        <p>Paragraph three.</p>
      </span>'
      `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('s1', bodyHtml));
      this.triggerReadSelectedText();

      // Navigates to the next paragraph and speaks until the second word.
      await selectToSpeak.onNextParagraphRequested();
      this.mockTts.speakUntilCharIndex(10);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Paragraph two.');

      // Hitting pause and resume will start reading the remaining content
      // in the second paragraph.
      selectToSpeak.onPauseRequested();
      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);
      selectToSpeak.onResumeRequested();
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'two.');

      // Should not keep reading beyond the second paragraph.
      this.mockTts.finishPendingUtterance();
      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'PauseResumeAfterSentenceNavigation',
    async function() {
      const bodyHtml = `
      <span id='s1'>
        <p>Sentence one. Sentence two.</p>
        <p>Paragraph two.</p>
      </span>'
      `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('s1', bodyHtml));
      this.triggerReadSelectedText();
      // Navigates to the next sentence and speaks until the last word
      // (i.e., "two") in the first pargraph.
      await selectToSpeak.onNextSentenceRequested();
      this.mockTts.speakUntilCharIndex(23);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Sentence two.');

      // Hitting pause and resume will start reading the remaining content
      // in the first paragraph.
      selectToSpeak.onPauseRequested();
      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);
      selectToSpeak.onResumeRequested();
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'two.');

      // Should not keep reading beyond the first paragraph.
      this.mockTts.finishPendingUtterance();
      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'PauseResumeAtTheEndOfNodeGroupItem',
    async function() {
      const bodyHtml = `
        <p id="p1">Sentence <span>one</span>. Sentence two.</p>
      `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml));
      this.triggerReadSelectedText();

      // Finishes the second word.
      this.mockTts.speakUntilCharIndex(13);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Sentence one . Sentence two.');

      // Hitting pause will stop the current TTS.
      selectToSpeak.onPauseRequested();
      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);

      // Hitting resume will start from the remaining content of the
      // paragraph.
      selectToSpeak.onResumeRequested();
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], '. Sentence two.');
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'PauseResumeFromKeystrokeSelection',
    async function() {
      const bodyHtml =
          '<p>This is some <b>bold</b> text</p><p>Second paragraph</p>';
      const setFocusCallback = this.newCallback(root => {
        const firstNode = this.findTextNode(root, 'This is some ');
        const lastNode = this.findTextNode(root, 'Second paragraph');
        // Sets the selection from "is some" to "Second".
        chrome.automation.setDocumentSelection({
          anchorObject: firstNode,
          anchorOffset: 5,
          focusObject: lastNode,
          focusOffset: 6,
        });
      });
      const root = await this.runWithLoadedTree(bodyHtml);
      root.addEventListener(
          'documentSelectionChanged', this.newCallback(function(event) {
            this.triggerReadSelectedText();

            // Speaks the first word 'is', the char index will count from the
            // beginning of the node (i.e., from "This").
            this.mockTts.speakUntilCharIndex(8);
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'is some bold text');

            // Hitting pause will stop the current TTS.
            selectToSpeak.onPauseRequested();
            assertFalse(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 0);

            // Hitting resume will start from the remaining content of the
            // paragraph.
            selectToSpeak.onResumeRequested();
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'some bold text');

            // Keep reading will finish all the content.
            this.mockTts.finishPendingUtterance();
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'Second');
          }),
          false);
      setFocusCallback(root);
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'NextSentence', async function() {
      const bodyHtml = `
      <p id="p1">This is the first. This is the second.</p>'
    `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml));
      this.triggerReadSelectedText();

      // Speaks the first word.
      this.mockTts.speakUntilCharIndex(5);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0],
          'This is the first. This is the second.');

      // Hitting next sentence will start another TTS.
      await selectToSpeak.onNextSentenceRequested();
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'This is the second.');
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'NextSentenceWithinParagraph',
    async function() {
      const bodyHtml = `
        <p id="p1">Sent 1. <span id="s1">Sent 2.</span> Sent 3. Sent 4.</p>
      `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('s1', bodyHtml));
      this.triggerReadSelectedText();

      // Speaks the first word.
      this.mockTts.speakUntilCharIndex(5);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Sent 2.');

      // Hitting next sentence will start from the next sentence.
      selectToSpeak.onNextSentenceRequested();
      this.waitOneEventLoop(() => {
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], 'Sent 3. Sent 4.');
      });
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'NextSentenceAcrossParagraph',
    async function() {
      const bodyHtml = `
        <p id="p1">Sent 1.</p>
        <p id="p2">Sent 2. Sent 3.</p>'
      `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml));
      this.triggerReadSelectedText();

      // Speaks the first word.
      this.mockTts.speakUntilCharIndex(5);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Sent 1.');

      // Hitting next sentence will star from the next paragraph as there
      // is no more sentence in the current paragraph.
      selectToSpeak.onNextSentenceRequested();
      this.waitOneEventLoop(() => {
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], 'Sent 2. Sent 3.');
      });
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'PrevSentence', async function() {
      const bodyHtml = `
      <p id="p1">First sentence. Second sentence. Third sentence.</p>'
    `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml));
      this.triggerReadSelectedText();

      // Speaks util the start of the second sentence.
      this.mockTts.speakUntilCharIndex(33);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0],
          'First sentence. Second sentence. Third sentence.');

      // Hitting prev sentence will start another TTS.
      await selectToSpeak.onPreviousSentenceRequested();
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0],
          'Second sentence. Third sentence.');
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'PrevSentenceFromMiddleOfSentence',
    async function() {
      const bodyHtml = `
      <p id="p1">First sentence. Second sentence. Third sentence.</p>'
    `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml));
      this.triggerReadSelectedText();

      // Speaks util the start of "sentence" in "Second sentence".
      this.mockTts.speakUntilCharIndex(23);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0],
          'First sentence. Second sentence. Third sentence.');

      // Hitting prev sentence will start another TTS.
      await selectToSpeak.onPreviousSentenceRequested();
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0],
          'First sentence. Second sentence. Third sentence.');
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'PrevSentenceWithinParagraph',
    async function() {
      const bodyHtml = `
      <p id="p1">Sent 0. Sent 1. <span id="s1">Sent 2.</span> Sent 3.</p>
    `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('s1', bodyHtml));
      this.triggerReadSelectedText();

      // Supposing we are at the start of the sentence.
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Sent 2.');

      // Hitting previous sentence will start from the previous sentence.
      selectToSpeak.onPreviousSentenceRequested();
      this.waitOneEventLoop(() => {
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], 'Sent 1. Sent 2. Sent 3.');
      });
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'PrevSentenceAcrossParagraph',
    async function() {
      const bodyHtml = `
      <p id="p1">Sent 1. Sent 2.</p>
      <p id="p2">Sent 3.</p>'
    `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p2', bodyHtml));
      this.triggerReadSelectedText();

      // We are at the start of the sentence.
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Sent 3.');

      // Hitting previous sentence will start from the last sentence in
      // the previous paragraph as there is no more sentence in the
      // current paragraph.
      selectToSpeak.onPreviousSentenceRequested();
      this.waitOneEventLoop(() => {
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], 'Sent 2.');
      });
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'ChangeSpeedWhilePlaying',
    async function() {
      chrome.settingsPrivate.setPref('settings.tts.speech_rate', 1.2);
      const bodyHtml = `
      <p id="p1">Paragraph 1</p>'
    `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml));
      this.triggerReadSelectedText();

      // Speaks the first word.
      this.mockTts.speakUntilCharIndex(10);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Paragraph 1');
      assertEquals(this.mockTts.getOptions().rate, 1.2);

      // Changing speed will resume with the remaining content of the
      // current sentence.
      selectToSpeak.onChangeSpeedRequested(1.5);
      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);

      // Wait an event loop so all pending promises are resolved prior to
      // asserting that TTS resumed with the proper rate.
      setTimeout(
          this.newCallback(() => {
            // Should resume TTS with the remaining content with adjusted
            // rate.
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.getOptions().rate, 1.8);
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], '1');
          }),
          0);
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'RetainsSpeedChange',
    async function() {
      chrome.settingsPrivate.setPref('settings.tts.speech_rate', 1.0);
      const bodyHtml = `
    <p id="p1">Paragraph 1</p>'
  `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml));
      this.triggerReadSelectedText();

      // Changing speed then exit.
      selectToSpeak.onChangeSpeedRequested(1.5);
      selectToSpeak.onExitRequested();
      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);

      // Next TTS session should remember previous rate.
      this.triggerReadSelectedText();
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.getOptions().rate, 1.5);
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'ChangeSpeedWhilePaused',
    async function() {
      chrome.settingsPrivate.setPref('settings.tts.speech_rate', 1.2);
      const bodyHtml = `
      <p id="p1">Paragraph 1</p>'
    `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml));
      this.triggerReadSelectedText();

      // Speaks the first word.
      this.mockTts.speakUntilCharIndex(10);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Paragraph 1');
      assertEquals(this.mockTts.getOptions().rate, 1.2);

      // User-intiated pause.
      selectToSpeak.onPauseRequested();
      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);

      // Changing speed will remain paused.
      selectToSpeak.onChangeSpeedRequested(1.5);

      // Wait an event loop so all pending promises are resolved prior to
      // asserting that TTS remains paused.
      setTimeout(this.newCallback(() => {
        assertFalse(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 0);
      }, 0));
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'ResumeAtTheEndOfParagraph',
    async function() {
      const bodyHtml = `
        <p id="p1">Paragraph 1</p>
        <p id="p2">Paragraph 2</p>
      `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml));
      this.triggerReadSelectedText();

      // Finishes the current utterance.
      this.mockTts.finishPendingUtterance();

      // Hitting resume will start the next paragraph.
      selectToSpeak.onResumeRequested();
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Paragraph 2');
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'ResumeAtTheEndOfUserSelection',
    async function() {
      const bodyHtml = `
        <p id="p1">Sentence <span id="s1">one</span>. Sentence two.</p>
        <p id="p2">Paragraph 2</p>
      `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('s1', bodyHtml));
      this.triggerReadSelectedText();

      // Finishes the current utterance.
      this.mockTts.finishPendingUtterance();

      // Hitting resume will start the remaining content.
      selectToSpeak.onResumeRequested();
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], '. Sentence two.');
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'ResumeFromSelectionEndingInSpace',
    async function() {
      const bodyHtml = '<p>This is some text with space.</p>';
      const setFocusCallback = this.newCallback(root => {
        const node = this.findTextNode(root, 'This is some text with space.');
        // Sets the selection to "This ".
        chrome.automation.setDocumentSelection({
          anchorObject: node,
          anchorOffset: 0,
          focusObject: node,
          focusOffset: 5,
        });
      });
      const root = await this.runWithLoadedTree(bodyHtml);
      root.addEventListener(
          'documentSelectionChanged', this.newCallback(event => {
            this.triggerReadSelectedText();

            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'This');

            // Finishes the current utterance.
            this.mockTts.finishPendingUtterance();

            // Hitting resume will start from the remaining content of the
            // paragraph.
            selectToSpeak.onResumeRequested();
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0],
                'is some text with space.');
          }),
          false);
      setFocusCallback(root);
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'ResizeWhilePlaying',
    async function() {
      const longLine =
          'Second paragraph is longer than 300 pixels and will wrap when' +
          'resized';
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
      const root = await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('content', bodyHtml));
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
      resizeButton.addEventListener(EventType.CLICKED, this.newCallback(() => {
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

AX_TEST_F(
    'SelectToSpeakNavigationControlTest',
    'RemainsActiveAfterCompletingUtterance', async function() {
      const bodyHtml = '<p id="p1">Paragraph 1</p>';
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml));
      // Simulate starting and completing TTS.
      this.triggerReadSelectedText();
      this.mockTts.finishPendingUtterance();

      // Should remain in speaking state.
      assertEquals(selectToSpeak.state_, SelectToSpeakState.SPEAKING);
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest',
    'AutoDismissesIfNavigationControlsDisabled', async function() {
      const bodyHtml = '<p id="p1">Paragraph 1</p>';
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml));

      // Disable navigation controls.
      selectToSpeak.prefsManager_.navigationControlsEnabled_ = false;

      // Simulate starting and completing TTS.
      this.triggerReadSelectedText();
      this.mockTts.finishPendingUtterance();

      // Should auto-dismiss.
      assertEquals(selectToSpeak.state_, SelectToSpeakState.INACTIVE);
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'NavigatesToNextParagraphQuickly',
    async function() {
      const bodyHtml = `
        <p id="p1">Paragraph 1</p>
        <p id="p2">Paragraph 2</p>'
      `;
      await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml));
      // Have mock TTS engine wait to send events so we can simulate a
      // delayed 'start' event.
      this.mockTts.setWaitToSendEvents(true);
      this.triggerReadSelectedText();
      const speakOptions = this.mockTts.getOptions();

      // Navigate to next paragraph before speech begins.
      selectToSpeak.onNextParagraphRequested();

      this.waitOneEventLoop(() => {
        // Manually triggered delayed events.
        this.mockTts.sendPendingEvents();

        // Should remain in speaking state.
        assertEquals(selectToSpeak.state_, SelectToSpeakState.SPEAKING);
      });
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'SetsInitialFocusToPanel',
    async function() {
      const bodyHtml = '<p id="p1">Sample text</p>';
      const root = await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml));
      const desktop = root.parent.root;

      // Wait for button in STS panel to be focused.
      // Test will fail if panel is never focused.
      this.waitForPanelFocus(desktop, () => {});

      // Trigger STS, which will initially set focus to the panel.
      this.triggerReadSelectedText();
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'KeyboardShortcutKeepsFocusInPanel',
    async function() {
      const bodyHtml = '<p id="p1">Sample text</p>';
      const root = await this.runWithLoadedTree(
          this.generateHtmlWithSelectedElement('p1', bodyHtml));
      const desktop = root.parent.root;

      // Wait for button within STS panel is focused.
      this.waitForPanelFocus(desktop, () => {
        // Remove text selection.
        const textNode = this.findTextNode(root, 'Sample text');
        chrome.automation.setDocumentSelection({
          anchorObject: textNode,
          anchorOffset: 0,
          focusObject: textNode,
          focusOffset: 0,
        });

        // Perform Search key + S, which should restore focus to
        // panel.
        selectToSpeak.sendMockSelectToSpeakKeysPressedChanged(
            [SelectToSpeakConstants.SEARCH_KEY_CODE]);
        selectToSpeak.sendMockSelectToSpeakKeysPressedChanged([
          SelectToSpeakConstants.SEARCH_KEY_CODE,
          SelectToSpeakConstants.READ_SELECTION_KEY_CODE,
        ]);
        selectToSpeak.sendMockSelectToSpeakKeysPressedChanged(
            [SelectToSpeakConstants.SEARCH_KEY_CODE]);
        selectToSpeak.sendMockSelectToSpeakKeysPressedChanged([]);

        // Verify focus is still on button within panel.
        chrome.automation.getFocus(this.newCallback(focusedNode => {
          assertEquals(focusedNode.role, RoleType.TOGGLE_BUTTON);
          assertTrue(this.isNodeWithinPanel(focusedNode));
        }));
      });

      // Trigger STS, which will initially set focus to the panel.
      this.triggerReadSelectedText();
    });

AX_TEST_F(
    'SelectToSpeakNavigationControlTest', 'SelectingWindowDoesNotShowPanel',
    async function() {
      const bodyHtml = `
        <title>Test</title>
        <div style="position: absolute; top: 300px;">
          Hello
        </div>
      `;
      const root = await this.runWithLoadedTree(bodyHtml);
      // Expect call to updateSelectToSpeakPanel to set panel to be hidden.
      chrome.accessibilityPrivate.updateSelectToSpeakPanel =
          this.newCallback(visible => {
            assertFalse(visible);
          });

      // Trigger mouse selection on a part of the page where no text nodes
      // exist, should select entire page.
      const mouseEvent = {
        screenX: root.location.left + 1,
        screenY: root.location.top + 1,
      };
      this.triggerReadMouseSelectedText(mouseEvent, mouseEvent);
    });
