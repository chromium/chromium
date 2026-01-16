// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['select_to_speak_e2e_test_base.js']);
GEN_INCLUDE(['../common/testing/mock_tts.js']);

/**
 * Browser tests for select-to-speak's feature to speak text
 * at the press of a keystroke.
 */
SelectToSpeakKeystrokeSelectionTest = class extends SelectToSpeakE2ETest {
  constructor() {
    super();
    this.mockTts = new MockTts();
    chrome.tts = this.mockTts;
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

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

  /**
   * Function to load a simple webpage, select some of the single text
   * node, and trigger Select-to-Speak to read that partial node. Tests
   * that the selected region creates tts output that matches the expected
   * output.
   * @param {string} text The text to load on the simple webpage.
   * @param {number} anchorOffset The offset into the text node where
   *     focus starts.
   * @param {number} focusOffset The offset into the text node where
   *     focus ends.
   * @param {string} expected The expected string that will be read, ignoring
   *     extra whitespace, after this selection is triggered.
   */
  async testSimpleTextAtKeystroke(text, anchorOffset, focusOffset, expected) {
    await this.testReadTextAtKeystroke(
        '<p>' + text + '</p>', async function(root) {
          // Set the document selection. This will fire the changed event
          // above, allowing us to do the keystroke and test that speech
          // occurred properly.
          const textNode = this.findTextNode(root, text);
          chrome.automation.setDocumentSelection({
            anchorObject: textNode,
            anchorOffset,
            focusObject: textNode,
            focusOffset,
          });
        }, expected);
  }

  /**
   * Function to load given html using a data url, have the caller set a
   * selection on that page, and then trigger select-to-speak to read
   * the selected text. Tests that the tts output matches the expected
   * output.
   * @param {string} contents The web contents to load
   * @param {function(AutomationNode)} setFocusCallback Callback
   *     to take the root node and set the selection appropriately. Once
   *     selection is set, the test will listen for the focus set event and
   *     trigger select-to-speak, comparing the resulting tts output to what
   *     was expected.
   *     will trigger select-to-speak to speak any selected text
   * @param {string} expected The expected string that will be read, ignoring
   *     extra whitespace, after this selection is triggered.
   */
  async testReadTextAtKeystroke(contents, setFocusCallback, expected) {
    setFocusCallback = this.newCallback(setFocusCallback);
    const root = await this.runWithLoadedTree(contents);
    // Set the selection.
    setFocusCallback(root);
    // Wait for Automation to update.
    await this.waitForEvent(
        root, 'documentSelectionChanged', /*capture=*/ false);
    // Speak selected text.
    await this.triggerReadSelectedText(root);
    await this.waitForSpeech();
    assertEquals(this.mockTts.pendingUtterances().length, 1);
    this.assertEqualsCollapseWhitespace(
        this.mockTts.pendingUtterances()[0], expected);
  }

  generateHtmlWithSelection(selectionCode, bodyHtml) {
    return '<script type="text/javascript">' +
        'function doSelection() {' +
        'let selection = window.getSelection();' +
        'let range = document.createRange();' +
        'selection.removeAllRanges();' + selectionCode +
        'selection.addRange(range);}' +
        '</script>' +
        '<body onload="doSelection()">' + bodyHtml + '</body>';
  }

  /**
   * Function to set the value property and the text selection properties of
   * the given node using a text value, a start index, and an end index. It
   * keeps trying to set and wait for textSelStart and textSelEnd until these
   * text selection properties are set with the given indices, respectively.
   * @param {AutomationNode} node The automation node to be set.
   * @param {string} text The text to be set to the node's value property.
   * @param {number} startIndex The index in the text field where focus starts.
   * @param {number} endIndex The index in the text field where focus ends.
   */
  async setValueAndTextSelection(node, value, startIndex, endIndex) {
    node.setValue(value);
    await this.waitForEvent(node, 'valueChanged');

    while (node.textSelStart !== startIndex || node.textSelEnd !== endIndex) {
      node.setSelection(startIndex, endIndex);
      await this.waitForEvent(node, 'textSelectionChanged');
    }
  }
};

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'SpeaksTextAtKeystrokeFullText',
    async function() {
      await this.testSimpleTextAtKeystroke(
          'This is some text', 0, 17, 'This is some text');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'SpeaksTextAtKeystrokePartialText',
    async function() {
      await this.testSimpleTextAtKeystroke(
          'This is some text', 0, 12, 'This is some');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'SpeaksTextAtKeystrokeSingleWord',
    async function() {
      await this.testSimpleTextAtKeystroke('This is some text', 8, 12, 'some');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'SpeaksTextAtKeystrokePartialWord',
    async function() {
      await this.testSimpleTextAtKeystroke('This is some text', 8, 10, 'so');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'SpeaksAcrossNodesAtKeystroke',
    async function() {
      await this.testReadTextAtKeystroke(
          '<p>This is some <b>bold</b> text</p><p>Second paragraph</p>',
          function(root) {
            const firstNode = this.findTextNode(root, 'This is some ');
            const lastNode = this.findTextNode(root, ' text');
            chrome.automation.setDocumentSelection({
              anchorObject: firstNode,
              anchorOffset: 0,
              focusObject: lastNode,
              focusOffset: 5,
            });
          },
          'This is some bold text');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest',
    'SpeaksAcrossNodesSelectedBackwardsAtKeystroke', async function() {
      await this.testReadTextAtKeystroke(
          '<p>This is some <b>bold</b> text</p><p>Second paragraph</p>',
          function(root) {
            // Set the document selection backwards in page order.
            const lastNode = this.findTextNode(root, 'This is some ');
            const firstNode = this.findTextNode(root, ' text');
            chrome.automation.setDocumentSelection({
              anchorObject: firstNode,
              anchorOffset: 5,
              focusObject: lastNode,
              focusOffset: 0,
            });
          },
          'This is some bold text');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'SpeakTextSurroundedByBrs',
    async function() {
      // If you load this html and double-click on "Selected text", this is the
      // document selection that occurs -- into the second <br/> element.

      let setFocusCallback = function(root) {
        const firstNode = this.findTextNode(root, 'Selected text');
        const lastNode = root.findAll({role: 'lineBreak'})[1];
        chrome.automation.setDocumentSelection({
          anchorObject: firstNode,
          anchorOffset: 0,
          focusObject: lastNode,
          focusOffset: 1,
        });
      };
      setFocusCallback = this.newCallback(setFocusCallback);
      const root =
          await this.runWithLoadedTree('<br/><p>Selected text</p><br/>');
      // Add an event listener that will start the user interaction
      // of the test once the selection is completed.
      root.addEventListener(
          'documentSelectionChanged', this.newCallback(async function(event) {
            await this.triggerReadSelectedText(root);
            assertTrue(this.mockTts.currentlySpeaking());
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'Selected text');

            this.mockTts.finishPendingUtterance();
            if (this.mockTts.pendingUtterances().length === 1) {
              this.assertEqualsCollapseWhitespace(
                  this.mockTts.pendingUtterances()[0], '');
            }
          }),
          false);
      setFocusCallback(root);
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'StartsReadingAtFirstNodeWithText',
    async function() {
      await this.testReadTextAtKeystroke(
          '<div id="empty"></div><div><p>This is some <b>bold</b> text</p></div>',
          function(root) {
            const firstNode =
                this.findTextNode(root, 'This is some ').root.children[0];
            const lastNode = this.findTextNode(root, ' text');
            chrome.automation.setDocumentSelection({
              anchorObject: firstNode,
              anchorOffset: 0,
              focusObject: lastNode,
              focusOffset: 5,
            });
          },
          'This is some bold text');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'IgnoresTextMarkedNotUserSelectable',
    async function() {
      await this.testReadTextAtKeystroke(
          '<div><p>This is some <span style="user-select:none">unselectable</span> text</p></div>',
          function(root) {
            const firstNode =
                this.findTextNode(root, 'This is some ').root.children[0];
            const lastNode = this.findTextNode(root, ' text');
            chrome.automation.setDocumentSelection({
              anchorObject: firstNode,
              anchorOffset: 0,
              focusObject: lastNode,
              focusOffset: 5,
            });
          },
          'This is some text');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest',
    'HandlesSingleImageCorrectlyWithAutomation', async function() {
      await this.testReadTextAtKeystroke(
          '<img src="pipe.jpg" alt="one"/>', function(root) {
            const container = root.findAll({role: 'genericContainer'})[0];
            chrome.automation.setDocumentSelection({
              anchorObject: container,
              anchorOffset: 0,
              focusObject: container,
              focusOffset: 1,
            });
          }, 'one');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest',
    'HandlesMultipleImagesCorrectlyWithAutomation', async function() {
      await this.testReadTextAtKeystroke(
          '<img src="pipe.jpg" alt="one"/>' +
              '<img src="pipe.jpg" alt="two"/><img src="pipe.jpg" alt="three"/>',
          function(root) {
            const container = root.findAll({role: 'genericContainer'})[0];
            chrome.automation.setDocumentSelection({
              anchorObject: container,
              anchorOffset: 1,
              focusObject: container,
              focusOffset: 2,
            });
          },
          'two');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest',
    'HandlesMultipleImagesCorrectlyWithJS1', async function() {
      // Using JS to do the selection instead of Automation, so that we can
      // ensure this is stable against changes in chrome.automation.
      const selectionCode =
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(body, 1);' +
          'range.setEnd(body, 2);';
      const root = await this.runWithLoadedTree(this.generateHtmlWithSelection(
          selectionCode,
          '<img id="one" src="pipe.jpg" alt="one"/>' +
              '<img id="two" src="pipe.jpg" alt="two"/>' +
              '<img id="three" src="pipe.jpg" alt="three"/>'));
      await this.triggerReadSelectedText(root);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'two');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest',
    'HandlesMultipleImagesCorrectlyWithJS2', async function() {
      const selectionCode =
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(body, 1);' +
          'range.setEnd(body, 3);';
      const root = await this.runWithLoadedTree(this.generateHtmlWithSelection(
          selectionCode,
          '<img id="one" src="pipe.jpg" alt="one"/>' +
              '<img id="two" src="pipe.jpg" alt="two"/>' +
              '<img id="three" src="pipe.jpg" alt="three"/>'));
      await this.triggerReadSelectedText(root);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'two three');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'TextFieldFullySelected',
    async function() {
      const selectionCode = 'let p = document.getElementsByTagName("p")[0];' +
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(p, 0);' +
          'range.setEnd(body, 2);';
      const root = await this.runWithLoadedTree(this.generateHtmlWithSelection(
          selectionCode,
          '<p>paragraph</p>' +
              '<input type="text" value="text field">'));
      await this.triggerReadSelectedText(root);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'paragraph');

      this.mockTts.finishPendingUtterance();
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'text field');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'TwoTextFieldsFullySelected',
    async function() {
      const selectionCode =
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(body, 0);' +
          'range.setEnd(body, 2);';
      const root = await this.runWithLoadedTree(this.generateHtmlWithSelection(
          selectionCode,
          '<input type="text" value="one"></input>' +
              '<textarea cols="5">two three</textarea>'));
      await this.triggerReadSelectedText(root);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'one');

      this.mockTts.finishPendingUtterance();
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'two three');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'TextInputPartiallySelected',
    async function() {
      const html = '<script type="text/javascript">' +
          'function doSelection() {' +
          'let input = document.getElementById("input");' +
          'input.focus();' +
          'input.setSelectionRange(5, 10);' +
          '}' +
          '</script>' +
          '<body onload="doSelection()">' +
          '<input id="input" type="text" value="text field"></input>' +
          '</body>';
      const root = await this.runWithLoadedTree(html);
      await this.triggerReadSelectedText(root);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'field');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'TextAreaPartiallySelected',
    async function() {
      const html = '<script type="text/javascript">' +
          'function doSelection() {' +
          'let input = document.getElementById("input");' +
          'input.focus();' +
          'input.setSelectionRange(6, 17);' +
          '}' +
          '</script>' +
          '<body onload="doSelection()">' +
          '<textarea id="input" type="text" cols="10">first line second line</textarea>' +
          '</body>';
      const root = await this.runWithLoadedTree(html);
      await this.triggerReadSelectedText(root);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'line second');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'HandlesTextWithBr',
    async function() {
      const selectionCode =
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(body, 0);' +
          'range.setEnd(body, 3);';
      const root = await this.runWithLoadedTree(this.generateHtmlWithSelection(
          selectionCode, 'Test<br/><br/>Unread'));
      await this.triggerReadSelectedText(root);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Test');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'HandlesTextWithBrComplex',
    async function() {
      const selectionCode = 'let p = document.getElementsByTagName("p")[0];' +
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(p, 0);' +
          'range.setEnd(body, 2);';
      const root = await this.runWithLoadedTree(this.generateHtmlWithSelection(
          selectionCode, '<p>Some text</p><br/><br/>Unread'));
      await this.triggerReadSelectedText(root);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Some text');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'HandlesTextWithBrAfterText1',
    async function() {
      // A bug was that if the selection was on the rootWebArea, paragraphs were
      // not counted correctly. The more divs and paragraphs before the
      // selection, the further off it got.
      const selectionCode = 'let p = document.getElementsByTagName("p")[0];' +
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(p, 1);' +
          'range.setEnd(body, 2);';
      const root = await this.runWithLoadedTree(this.generateHtmlWithSelection(
          selectionCode, '<p>Unread</p><p>Some text</p><br/>Unread'));
      await this.triggerReadSelectedText(root);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Some text');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'HandlesTextWithBrAfterText2',
    async function() {
      // A bug was that if the selection was on the rootWebArea, paragraphs were
      // not counted correctly. The more divs and paragraphs before the
      // selection, the further off it got.
      const selectionCode = 'let p = document.getElementsByTagName("p")[0];' +
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(p, 1);' +
          'range.setEnd(body, 3);';
      const root = await this.runWithLoadedTree(this.generateHtmlWithSelection(
          selectionCode, '<p>Unread</p><p>Some text</p><br/>Unread'));
      await this.triggerReadSelectedText(root);
      assertTrue(this.mockTts.currentlySpeaking());
      assertTrue(this.mockTts.pendingUtterances().length > 0);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Some text');

      this.mockTts.finishPendingUtterance();
      if (this.mockTts.pendingUtterances().length > 0) {
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], '');
      }
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'HandlesTextAreaAndBrs',
    async function() {
      const selectionCode =
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(body, 1);' +
          'range.setEnd(body, 4);';
      const root = await this.runWithLoadedTree(this.generateHtmlWithSelection(
          selectionCode,
          '<br/><br/><textarea>Some text</textarea><br/><br/>Unread'));
      await this.triggerReadSelectedText(root);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Some text');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'textFieldWithComboBoxSimple',
    async function() {
      const selectionCode =
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(body, 0);' +
          'range.setEnd(body, 1);';
      const root = await this.runWithLoadedTree(this.generateHtmlWithSelection(
          selectionCode,
          '<input list="list" value="one"></label><datalist id="list">' +
              '<option value="one"></datalist>'));
      await this.triggerReadSelectedText(root);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'one');
    });
// TODO(katie): It doesn't seem possible to programatically specify a range that
// selects only part of the text in a combo box.

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'ContentEditableInternallySelected',
    async function() {
      const html = '<script type="text/javascript">' +
          'function doSelection() {' +
          'let input = document.getElementById("input");' +
          'input.focus();' +
          'let selection = window.getSelection();' +
          'let range = document.createRange();' +
          'selection.removeAllRanges();' +
          'let p1 = document.getElementsByTagName("p")[0];' +
          'let p2 = document.getElementsByTagName("p")[1];' +
          'range.setStart(p1.firstChild, 1);' +
          'range.setEnd(p2.firstChild, 3);' +
          'selection.addRange(range);' +
          '}' +
          '</script>' +
          '<body onload="doSelection()">' +
          '<div id="input" contenteditable><p>a b c</p><p>d e f</p></div>' +
          '</body>';
      const root = await this.runWithLoadedTree(html);
      await this.triggerReadSelectedText(root);
      assertTrue(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'b c');

      this.mockTts.finishPendingUtterance();
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'd e');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'ContentEditableExternallySelected',
    async function() {
      const selectionCode =
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(body, 1);' +
          'range.setEnd(body, 2);';
      const root = await this.runWithLoadedTree(this.generateHtmlWithSelection(
          selectionCode,
          'Unread <div id="input" contenteditable><p>a b c</p><p>d e f</p>' +
              '</div> Unread'));
      await this.triggerReadSelectedText(root);
      assertTrue(this.mockTts.currentlySpeaking());
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'a b c');

      this.mockTts.finishPendingUtterance();
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'd e f');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'ReordersSvgSingleLine',
    async function() {
      const selectionCode =
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(body, 0);' +
          'range.setEnd(body, 1);';
      const root = await this.runWithLoadedTree(this.generateHtmlWithSelection(
          selectionCode,
          '<svg viewBox="0 0 240 80" xmlns="http://www.w3.org/2000/svg">' +
              '  <text x="65" y="55">Grumpy!</text>' +
              '  <text x="20" y="35">My</text>' +
              '  <text x="40" y="35">cat</text>' +
              '  <text x="55" y="55">is</text>' +
              '</svg>'));
      await this.triggerReadSelectedText(root);
      assertTrue(this.mockTts.currentlySpeaking());
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'My cat is Grumpy!');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'ReordersSvgWithGroups',
    async function() {
      const selectionCode =
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(body, 0);' +
          'range.setEnd(body, 1);';
      const root = await this.runWithLoadedTree(this.generateHtmlWithSelection(
          selectionCode,
          '<svg viewBox="0 0 240 80" xmlns="http://www.w3.org/2000/svg">' +
              '  <g>' +
              '    <text x="65" y="0">Column 2, Text 1</text>' +
              '    <text x="65" y="50">Column 2, Text 2</text>' +
              '  </g>' +
              '  <g>' +
              '    <text x="0" y="50">Column 1, Text 2</text>' +
              '    <text x="0" y="0">Column 1, Text 1</text>' +
              '  </g>' +
              '</svg>'));
      await this.triggerReadSelectedText(root);
      assertTrue(this.mockTts.currentlySpeaking());
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Column 1, Text 1');

      this.mockTts.finishPendingUtterance();
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Column 1, Text 2');

      this.mockTts.finishPendingUtterance();
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Column 2, Text 1');

      this.mockTts.finishPendingUtterance();
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Column 2, Text 2');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest',
    'NonReorderedSvgPreservesSelectionStartEnd', async function() {
      const selectionCode = 'const t1 = document.getElementById("t1");' +
          'const t2 = document.getElementById("t2");' +
          'range.setStart(t1.childNodes[0], 3);' +
          'range.setEnd(t2.childNodes[0], 2);';
      const root = await this.runWithLoadedTree(this.generateHtmlWithSelection(
          selectionCode,
          '<svg viewBox="0 0 240 80" xmlns="http://www.w3.org/2000/svg">' +
              '  <text id="t1" x="0" y="55">My cat</text>' +
              '  <text id="t2" x="100" y="55">is Grumpy!</text>' +
              '</svg>'));
      await this.triggerReadSelectedText(root);
      assertTrue(this.mockTts.currentlySpeaking());
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'cat is');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest',
    'ReorderedSvgIgnoresSelectionStartEnd', async function() {
      const selectionCode = 'const t1 = document.getElementById("t1");' +
          'const t2 = document.getElementById("t2");' +
          'range.setStart(t1.childNodes[0], 3);' +
          'range.setEnd(t2.childNodes[0], 2);';
      const root = await this.runWithLoadedTree(this.generateHtmlWithSelection(
          selectionCode,
          '<svg viewBox="0 0 240 80" xmlns="http://www.w3.org/2000/svg">' +
              '  <text id="t1" x="100" y="55">is Grumpy!</text>' +
              '  <text id="t2" x="0" y="55">My cat</text>' +
              '</svg>'));
      await this.triggerReadSelectedText(root);
      assertTrue(this.mockTts.currentlySpeaking());
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'My cat is Grumpy!');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'OmniboxFullySelected',
    async function() {
      let omnibox;
      await this.runWithLoadedDesktop(desktop => {
        omnibox = desktop.find({attributes: {className: 'OmniboxViewViews'}});
      });

      await this.setValueAndTextSelection(
          omnibox, 'Hello, Chromium a11y', 0, 20);
      assertEquals('Hello, Chromium a11y', omnibox.value);
      assertEquals(0, omnibox.textSelStart);
      assertEquals(20, omnibox.textSelEnd);

      await this.triggerReadSelectedText();
      assertTrue(this.mockTts.currentlySpeaking());
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Hello, Chromium a11y');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'OmniboxPartiallySelectedFromStart',
    async function() {
      let omnibox;
      await this.runWithLoadedDesktop(desktop => {
        omnibox = desktop.find({attributes: {className: 'OmniboxViewViews'}});
      });

      await this.setValueAndTextSelection(
          omnibox, 'Hello, Chromium a11y', 0, 5);
      assertEquals('Hello, Chromium a11y', omnibox.value);
      assertEquals(0, omnibox.textSelStart);
      assertEquals(5, omnibox.textSelEnd);

      await this.triggerReadSelectedText();
      assertTrue(this.mockTts.currentlySpeaking());
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Hello');
    });


AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'OmniboxPartiallySelectedToEnd',
    async function() {
      let omnibox;
      const root = await this.runWithLoadedDesktop(desktop => {
        omnibox = desktop.find({attributes: {className: 'OmniboxViewViews'}});
      });

      await this.setValueAndTextSelection(
          omnibox, 'Hello, Chromium a11y', 7, 20);
      assertEquals('Hello, Chromium a11y', omnibox.value);
      assertEquals(7, omnibox.textSelStart);
      assertEquals(20, omnibox.textSelEnd);

      await this.triggerReadSelectedText(root);
      assertTrue(this.mockTts.currentlySpeaking());
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Chromium a11y');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'OmniboxPartiallySelectedInMid',
    async function() {
      let omnibox;
      await this.runWithLoadedDesktop(desktop => {
        omnibox = desktop.find({attributes: {className: 'OmniboxViewViews'}});
      });

      await this.setValueAndTextSelection(
          omnibox, 'Hello, Chromium a11y', 7, 15);
      assertEquals('Hello, Chromium a11y', omnibox.value);
      assertEquals(7, omnibox.textSelStart);
      assertEquals(15, omnibox.textSelEnd);

      await this.triggerReadSelectedText();
      assertTrue(this.mockTts.currentlySpeaking());
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'Chromium');
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'OmniboxNoneSelected',
    async function() {
      let omnibox;
      await this.runWithLoadedDesktop(desktop => {
        omnibox = desktop.find({attributes: {className: 'OmniboxViewViews'}});
      });

      await this.setValueAndTextSelection(
          omnibox, 'Hello, Chromium a11y', 0, 0);
      assertEquals('Hello, Chromium a11y', omnibox.value);
      assertEquals(0, omnibox.textSelStart);
      assertEquals(0, omnibox.textSelEnd);

      await this.triggerReadSelectedText();
      assertEquals(false, this.mockTts.currentlySpeaking());
    });

AX_TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'SearchUpBeforeS', async function() {
      // SelectToSpeakE2ETest.triggerReadSelectedText releases the 'S' key
      // before the 'SEARCH' key.
      // This test releases 'SEARCH' before 'S' to ensure that speech is still
      // started.
      const setFocusCallback = this.newCallback(async function(root) {
        // Set the document selection. This will fire the changed event
        // above, allowing us to do the keystroke and test that speech
        // occurred properly.
        const textNode = this.findTextNode(root, 'This is some text');
        chrome.automation.setDocumentSelection({
          anchorObject: textNode,
          anchorOffset: 0,
          focusObject: textNode,
          focusOffset: 12,
        });
      });

      const root = await this.runWithLoadedTree('<p>This is some text</p>');
      // Set the selection.
      setFocusCallback(root);
      // Wait for Automation to update.
      await this.waitForEvent(
          root, 'documentSelectionChanged', /*capture=*/ false);
      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);

      // Speak selected text lifting the 'search' key before the 's' key.
      selectToSpeak.sendMockSelectToSpeakKeysPressedChanged(
          [SelectToSpeakConstants.SEARCH_KEY_CODE]);
      selectToSpeak.sendMockSelectToSpeakKeysPressedChanged([
        SelectToSpeakConstants.SEARCH_KEY_CODE,
        SelectToSpeakConstants.READ_SELECTION_KEY_CODE,
      ]);
      assertTrue(selectToSpeak.inputHandler_.isSelectionKeyDown_);

      // Release the SEARCH_KEY_CODE.
      selectToSpeak.sendMockSelectToSpeakKeysPressedChanged(
          [SelectToSpeakConstants.READ_SELECTION_KEY_CODE]);
      selectToSpeak.sendMockSelectToSpeakKeysPressedChanged([]);

      await this.waitForSpeech();
      assertEquals(this.mockTts.pendingUtterances().length, 1);
      this.assertEqualsCollapseWhitespace(
          this.mockTts.pendingUtterances()[0], 'This is some');
    });
