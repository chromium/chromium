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
