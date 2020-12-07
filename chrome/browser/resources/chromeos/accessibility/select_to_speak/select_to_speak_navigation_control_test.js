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
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'Paragraph 2');
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
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'Paragraph 1');
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

TEST_F('SelectToSpeakNavigationControlTest', 'PauseAndResume', function() {
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

        // Hitting pause will stop the current TTS.
        selectToSpeak.onSelectToSpeakPanelAction_(
            chrome.accessibilityPrivate.SelectToSpeakPanelAction.PAUSE);
        assertFalse(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 0);

        // Hitting resume will start from the next position.
        selectToSpeak.onSelectToSpeakPanelAction_(
            chrome.accessibilityPrivate.SelectToSpeakPanelAction.RESUME);
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], '1');
      });
});
