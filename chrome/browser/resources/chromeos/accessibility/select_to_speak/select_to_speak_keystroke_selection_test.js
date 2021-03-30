// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['select_to_speak_e2e_test_base.js']);
GEN_INCLUDE(['mock_tts.js']);

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
  setUp() {
    var runTest = this.deferRunTest(WhenTestDone.EXPECT);
    (async function() {
      await importModule(
          'selectToSpeak', '/select_to_speak/select_to_speak_main.js');
      await importModule(
          'SelectToSpeakConstants',
          '/select_to_speak/select_to_speak_constants.js');
      runTest();
    })();
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
  testSimpleTextAtKeystroke(text, anchorOffset, focusOffset, expected) {
    this.testReadTextAtKeystroke('<p>' + text + '</p>', function(root) {
      // Set the document selection. This will fire the changed event
      // above, allowing us to do the keystroke and test that speech
      // occurred properly.
      const textNode = this.findTextNode(root, 'This is some text');
      chrome.automation.setDocumentSelection({
        anchorObject: textNode,
        anchorOffset,
        focusObject: textNode,
        focusOffset
      });
    }, expected);
  }

  /**
   * Function to load given html using a data url, have the caller set a
   * selection on that page, and then trigger select-to-speak to read
   * the selected text. Tests that the tts output matches the expected
   * output.
   * @param {string} contents The web contents to load
   * @param {function(AutomationNode)} setSelectionCallback Callback
   *     to take the root node and set the selection appropriately. Once
   *     selection is set, the test will listen for the focus set event and
   *     trigger select-to-speak, comparing the resulting tts output to what
   *     was expected.
   *     will trigger select-to-speak to speak any selected text
   * @param {string} expected The expected string that will be read, ignoring
   *     extra whitespace, after this selection is triggered.
   */
  testReadTextAtKeystroke(contents, setFocusCallback, expected) {
    setFocusCallback = this.newCallback(setFocusCallback);
    this.runWithLoadedTree(contents, function(root) {
      // Add an event listener that will start the user interaction
      // of the test once the selection is completed.
      root.addEventListener(
          'documentSelectionChanged', this.newCallback(function(event) {
            this.triggerReadSelectedText();
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], expected);
          }),
          false);
      setFocusCallback(root);
    });
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
};

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'SpeaksTextAtKeystrokeFullText',
    function() {
      this.testSimpleTextAtKeystroke(
          'This is some text', 0, 17, 'This is some text');
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'SpeaksTextAtKeystrokePartialText',
    function() {
      this.testSimpleTextAtKeystroke(
          'This is some text', 0, 12, 'This is some');
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'SpeaksTextAtKeystrokeSingleWord',
    function() {
      this.testSimpleTextAtKeystroke('This is some text', 8, 12, 'some');
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'SpeaksTextAtKeystrokePartialWord',
    function() {
      this.testSimpleTextAtKeystroke('This is some text', 8, 10, 'so');
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'SpeaksAcrossNodesAtKeystroke',
    function() {
      this.testReadTextAtKeystroke(
          '<p>This is some <b>bold</b> text</p><p>Second paragraph</p>',
          function(root) {
            const firstNode = this.findTextNode(root, 'This is some ');
            const lastNode = this.findTextNode(root, ' text');
            chrome.automation.setDocumentSelection({
              anchorObject: firstNode,
              anchorOffset: 0,
              focusObject: lastNode,
              focusOffset: 5
            });
          },
          'This is some bold text');
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest',
    'SpeaksAcrossNodesSelectedBackwardsAtKeystroke', function() {
      this.testReadTextAtKeystroke(
          '<p>This is some <b>bold</b> text</p><p>Second paragraph</p>',
          function(root) {
            // Set the document selection backwards in page order.
            const lastNode = this.findTextNode(root, 'This is some ');
            const firstNode = this.findTextNode(root, ' text');
            chrome.automation.setDocumentSelection({
              anchorObject: firstNode,
              anchorOffset: 5,
              focusObject: lastNode,
              focusOffset: 0
            });
          },
          'This is some bold text');
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'SpeakTextSurroundedByBrs',
    function() {
      // If you load this html and double-click on "Selected text", this is the
      // document selection that occurs -- into the second <br/> element.

      let setFocusCallback = function(root) {
        const firstNode = this.findTextNode(root, 'Selected text');
        const lastNode = root.findAll({role: 'lineBreak'})[1];
        chrome.automation.setDocumentSelection({
          anchorObject: firstNode,
          anchorOffset: 0,
          focusObject: lastNode,
          focusOffset: 1
        });
      };
      setFocusCallback = this.newCallback(setFocusCallback);
      this.runWithLoadedTree(
          '<br/><p>Selected text</p><br/>', function(root) {
            // Add an event listener that will start the user interaction
            // of the test once the selection is completed.
            root.addEventListener(
                'documentSelectionChanged', this.newCallback(function(event) {
                  this.triggerReadSelectedText();
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
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'StartsReadingAtFirstNodeWithText',
    function() {
      this.testReadTextAtKeystroke(
          '<div id="empty"></div><div><p>This is some <b>bold</b> text</p></div>',
          function(root) {
            const firstNode =
                this.findTextNode(root, 'This is some ').root.children[0];
            const lastNode = this.findTextNode(root, ' text');
            chrome.automation.setDocumentSelection({
              anchorObject: firstNode,
              anchorOffset: 0,
              focusObject: lastNode,
              focusOffset: 5
            });
          },
          'This is some bold text');
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'IgnoresTextMarkedNotUserSelectable',
    function() {
      this.testReadTextAtKeystroke(
          '<div><p>This is some <span style="user-select:none">unselectable</span> text</p></div>',
          function(root) {
            const firstNode =
                this.findTextNode(root, 'This is some ').root.children[0];
            const lastNode = this.findTextNode(root, ' text');
            chrome.automation.setDocumentSelection({
              anchorObject: firstNode,
              anchorOffset: 0,
              focusObject: lastNode,
              focusOffset: 5
            });
          },
          'This is some text');
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest',
    'HandlesSingleImageCorrectlyWithAutomation', function() {
      this.testReadTextAtKeystroke(
          '<img src="pipe.jpg" alt="one"/>', function(root) {
            const container = root.findAll({role: 'genericContainer'})[0];
            chrome.automation.setDocumentSelection({
              anchorObject: container,
              anchorOffset: 0,
              focusObject: container,
              focusOffset: 1
            });
          }, 'one');
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest',
    'HandlesMultipleImagesCorrectlyWithAutomation', function() {
      this.testReadTextAtKeystroke(
          '<img src="pipe.jpg" alt="one"/>' +
              '<img src="pipe.jpg" alt="two"/><img src="pipe.jpg" alt="three"/>',
          function(root) {
            const container = root.findAll({role: 'genericContainer'})[0];
            chrome.automation.setDocumentSelection({
              anchorObject: container,
              anchorOffset: 1,
              focusObject: container,
              focusOffset: 2
            });
          },
          'two');
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest',
    'HandlesMultipleImagesCorrectlyWithJS1', function() {
      // Using JS to do the selection instead of Automation, so that we can
      // ensure this is stable against changes in chrome.automation.
      const selectionCode =
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(body, 1);' +
          'range.setEnd(body, 2);';
      this.runWithLoadedTree(
          this.generateHtmlWithSelection(
              selectionCode,
              '<img id="one" src="pipe.jpg" alt="one"/>' +
                  '<img id="two" src="pipe.jpg" alt="two"/>' +
                  '<img id="three" src="pipe.jpg" alt="three"/>'),
          function() {
            this.triggerReadSelectedText();
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'two');
          });
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest',
    'HandlesMultipleImagesCorrectlyWithJS2', function() {
      const selectionCode =
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(body, 1);' +
          'range.setEnd(body, 3);';
      this.runWithLoadedTree(
          this.generateHtmlWithSelection(
              selectionCode,
              '<img id="one" src="pipe.jpg" alt="one"/>' +
                  '<img id="two" src="pipe.jpg" alt="two"/>' +
                  '<img id="three" src="pipe.jpg" alt="three"/>'),
          function() {
            this.triggerReadSelectedText();
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'two three');
          });
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'TextFieldFullySelected',
    function() {
      const selectionCode = 'let p = document.getElementsByTagName("p")[0];' +
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(p, 0);' +
          'range.setEnd(body, 2);';
      this.runWithLoadedTree(
          this.generateHtmlWithSelection(
              selectionCode,
              '<p>paragraph</p>' +
                  '<input type="text" value="text field">'),
          function() {
            this.triggerReadSelectedText();
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'paragraph');

            this.mockTts.finishPendingUtterance();
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'text field');
          });
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'TwoTextFieldsFullySelected',
    function() {
      const selectionCode =
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(body, 0);' +
          'range.setEnd(body, 2);';
      this.runWithLoadedTree(
          this.generateHtmlWithSelection(
              selectionCode,
              '<input type="text" value="one"></input><textarea cols="5">two three</textarea>'),
          function() {
            this.triggerReadSelectedText();
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'one');

            this.mockTts.finishPendingUtterance();
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'two three');
          });
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'TextInputPartiallySelected',
    function() {
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
      this.runWithLoadedTree(html, function() {
        this.triggerReadSelectedText();
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], 'field');
      });
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'TextAreaPartiallySelected',
    function() {
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
      this.runWithLoadedTree(html, function() {
        this.triggerReadSelectedText();
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], 'line second');
      });
    });

TEST_F('SelectToSpeakKeystrokeSelectionTest', 'HandlesTextWithBr', function() {
  const selectionCode = 'let body = document.getElementsByTagName("body")[0];' +
      'range.setStart(body, 0);' +
      'range.setEnd(body, 3);';
  this.runWithLoadedTree(
      this.generateHtmlWithSelection(selectionCode, 'Test<br/><br/>Unread'),
      function() {
        this.triggerReadSelectedText();
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], 'Test');
      });
});

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'HandlesTextWithBrComplex',
    function() {
      const selectionCode = 'let p = document.getElementsByTagName("p")[0];' +
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(p, 0);' +
          'range.setEnd(body, 2);';
      this.runWithLoadedTree(
          this.generateHtmlWithSelection(
              selectionCode, '<p>Some text</p><br/><br/>Unread'),
          function() {
            this.triggerReadSelectedText();
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'Some text');
          });
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'HandlesTextWithBrAfterText1',
    function() {
      // A bug was that if the selection was on the rootWebArea, paragraphs were
      // not counted correctly. The more divs and paragraphs before the
      // selection, the further off it got.
      const selectionCode = 'let p = document.getElementsByTagName("p")[0];' +
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(p, 1);' +
          'range.setEnd(body, 2);';
      this.runWithLoadedTree(
          this.generateHtmlWithSelection(
              selectionCode, '<p>Unread</p><p>Some text</p><br/>Unread'),
          function() {
            this.triggerReadSelectedText();
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'Some text');
          });
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'HandlesTextWithBrAfterText2',
    function() {
      // A bug was that if the selection was on the rootWebArea, paragraphs were
      // not counted correctly. The more divs and paragraphs before the
      // selection, the further off it got.
      const selectionCode = 'let p = document.getElementsByTagName("p")[0];' +
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(p, 1);' +
          'range.setEnd(body, 3);';
      this.runWithLoadedTree(
          this.generateHtmlWithSelection(
              selectionCode, '<p>Unread</p><p>Some text</p><br/>Unread'),
          function() {
            this.triggerReadSelectedText();
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
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'HandlesTextAreaAndBrs', function() {
      const selectionCode =
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(body, 1);' +
          'range.setEnd(body, 4);';
      this.runWithLoadedTree(
          this.generateHtmlWithSelection(
              selectionCode,
              '<br/><br/><textarea>Some text</textarea><br/><br/>Unread'),
          function() {
            this.triggerReadSelectedText();
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'Some text');
          });
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'textFieldWithComboBoxSimple',
    function() {
      const selectionCode =
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(body, 0);' +
          'range.setEnd(body, 1);';
      this.runWithLoadedTree(
          this.generateHtmlWithSelection(
              selectionCode,
              '<input list="list" value="one"></label><datalist id="list">' +
                  '<option value="one"></datalist>'),
          function() {
            this.triggerReadSelectedText();
            assertTrue(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 1);
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'one');
          });
    });
// TODO(katie): It doesn't seem possible to programatically specify a range that
// selects only part of the text in a combo box.

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'contentEditableInternallySelected',
    function() {
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
      this.runWithLoadedTree(html, function() {
        this.triggerReadSelectedText();
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], 'b c');

        this.mockTts.finishPendingUtterance();
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], 'd e');
      });
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'contentEditableExternallySelected',
    function() {
      const selectionCode =
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(body, 1);' +
          'range.setEnd(body, 2);';
      this.runWithLoadedTree(
          this.generateHtmlWithSelection(
              selectionCode,
              'Unread <div id="input" contenteditable><p>a b c</p><p>d e f</p></div>' +
                  ' Unread'),
          function() {
            this.triggerReadSelectedText();
            assertTrue(this.mockTts.currentlySpeaking());
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'a b c');

            this.mockTts.finishPendingUtterance();
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'd e f');
          });
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'ReordersSvgSingleLine', function() {
      const selectionCode =
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(body, 0);' +
          'range.setEnd(body, 1);';
      this.runWithLoadedTree(
          this.generateHtmlWithSelection(
              selectionCode,
              '<svg viewBox="0 0 240 80" xmlns="http://www.w3.org/2000/svg">' +
                  '  <text x="65" y="55">Grumpy!</text>' +
                  '  <text x="20" y="35">My</text>' +
                  '  <text x="40" y="35">cat</text>' +
                  '  <text x="55" y="55">is</text>' +
                  '</svg>'),
          function() {
            this.triggerReadSelectedText();
            assertTrue(this.mockTts.currentlySpeaking());
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'My cat is Grumpy!');
          });
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest', 'ReordersSvgWithGroups', function() {
      const selectionCode =
          'let body = document.getElementsByTagName("body")[0];' +
          'range.setStart(body, 0);' +
          'range.setEnd(body, 1);';
      this.runWithLoadedTree(
          this.generateHtmlWithSelection(
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
                  '</svg>'),
          function() {
            this.triggerReadSelectedText();
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
    });
TEST_F(
    'SelectToSpeakKeystrokeSelectionTest',
    'NonReorderedSvgPreservesSelectionStartEnd', function() {
      const selectionCode = 'const t1 = document.getElementById("t1");' +
          'const t2 = document.getElementById("t2");' +
          'range.setStart(t1.childNodes[0], 3);' +
          'range.setEnd(t2.childNodes[0], 2);';
      this.runWithLoadedTree(
          this.generateHtmlWithSelection(
              selectionCode,
              '<svg viewBox="0 0 240 80" xmlns="http://www.w3.org/2000/svg">' +
                  '  <text id="t1" x="0" y="55">My cat</text>' +
                  '  <text id="t2" x="100" y="55">is Grumpy!</text>' +
                  '</svg>'),
          function() {
            this.triggerReadSelectedText();
            assertTrue(this.mockTts.currentlySpeaking());
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'cat is');
          });
    });

TEST_F(
    'SelectToSpeakKeystrokeSelectionTest',
    'ReorderedSvgIgnoresSelectionStartEnd', function() {
      const selectionCode = 'const t1 = document.getElementById("t1");' +
          'const t2 = document.getElementById("t2");' +
          'range.setStart(t1.childNodes[0], 3);' +
          'range.setEnd(t2.childNodes[0], 2);';
      this.runWithLoadedTree(
          this.generateHtmlWithSelection(
              selectionCode,
              '<svg viewBox="0 0 240 80" xmlns="http://www.w3.org/2000/svg">' +
                  '  <text id="t1" x="100" y="55">is Grumpy!</text>' +
                  '  <text id="t2" x="0" y="55">My cat</text>' +
                  '</svg>'),
          function() {
            this.triggerReadSelectedText();
            assertTrue(this.mockTts.currentlySpeaking());
            this.assertEqualsCollapseWhitespace(
                this.mockTts.pendingUtterances()[0], 'My cat is Grumpy!');
          });
    });
