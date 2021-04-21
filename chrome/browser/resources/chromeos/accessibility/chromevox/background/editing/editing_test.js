// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE([
  '//chrome/browser/resources/chromeos/accessibility/chromevox/testing/chromevox_next_e2e_test_base.js',
]);

/**
 * Test fixture for editing tests.
 */
ChromeVoxEditingTest = class extends ChromeVoxNextE2ETest {
  constructor() {
    super();
  }

  press(keyCode, modifiers) {
    return function() {
      EventGenerator.sendKeyPress(keyCode, modifiers);
    };
  }

  waitForEditableEvent() {
    return new Promise(resolve => {
      DesktopAutomationHandler.instance.textEditHandler_.onEvent = (e) =>
          resolve(e);
    });
  }
};


const doc = `
  <label for='singleLine'>singleLine</label>
  <input type='text' id='singleLine' value='Single line field'><br>
  <label for='textarea'>textArea</label>
  <textarea id='textarea'>
Line 1&#xa;
line 2&#xa;
line 3
</textarea>
`;

TEST_F('ChromeVoxEditingTest', 'Focus', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(doc, function(root) {
    const singleLine = root.find(
        {role: RoleType.TEXT_FIELD, attributes: {name: 'singleLine'}});
    const textarea =
        root.find({role: RoleType.TEXT_FIELD, attributes: {name: 'textArea'}});
    singleLine.focus();
    mockFeedback.expectSpeech('singleLine', 'Single line field', 'Edit text')
        .expectBraille(
            'singleLine Single line field ed', {startIndex: 11, endIndex: 11})
        .call(textarea.focus.bind(textarea))
        .expectSpeech('textArea', 'Line 1\nline 2\nline 3', 'Text area')
        .expectBraille(
            'textArea Line 1\nline 2\nline 3 mled',
            {startIndex: 9, endIndex: 9});

    mockFeedback.replay();
  });
});

TEST_F('ChromeVoxEditingTest', 'Multiline', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(doc, function(root) {
    const textarea =
        root.find({role: RoleType.TEXT_FIELD, attributes: {name: 'textArea'}});
    textarea.focus();
    mockFeedback.expectSpeech('textArea', 'Line 1\nline 2\nline 3', 'Text area')
        .expectBraille(
            'textArea Line 1\nline 2\nline 3 mled',
            {startIndex: 9, endIndex: 9})
        .call(textarea.setSelection.bind(textarea, 1, 1))
        .expectSpeech('i')
        .expectBraille('Line 1\nmled', {startIndex: 1, endIndex: 1})
        .call(textarea.setSelection.bind(textarea, 7, 7))
        .expectSpeech('line 2')
        .expectBraille('line 2\n', {startIndex: 0, endIndex: 0})
        .call(textarea.setSelection.bind(textarea, 7, 13))
        .expectSpeech('line 2', 'selected')
        .expectBraille('line 2\n', {startIndex: 0, endIndex: 6});

    mockFeedback.replay();
  });
});

TEST_F('ChromeVoxEditingTest', 'TextButNoSelectionChange', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(
      `
      <h1>Test doc</h1>
      <input type='text' id='input' value='text1'>
      <!-- We don't seem to get an event in js when the automation
           setSelection function is called, so poll for the actual change. -->

      <script>
        let timer;
        let input = document.getElementById('input');
        function poll(e) {
          if (input.selectionStart == 0) {
            return;
          }

          input.value = 'text2';
          window.clearInterval(timer);
        }
        timer = window.setInterval(poll, 200);
      </script>
    `,
      function(root) {
        const input = root.find({role: RoleType.TEXT_FIELD});
        input.focus();
        mockFeedback.expectSpeech('text1', 'Edit text')
            .expectBraille('text1 ed', {startIndex: 0, endIndex: 0})
            .call(input.setSelection.bind(input, 5, 5))
            .expectBraille('text2 ed', {startIndex: 5, endIndex: 5});

        mockFeedback.replay();
      });
});

TEST_F('ChromeVoxEditingTest', 'RichTextMoveByLine', function() {
  // Turn on rich text output settings.
  localStorage['announceRichTextAttributes'] = 'true';

  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(
      `
    <div role="textbox" contenteditable>
      <h2>hello</h2>
      <div><br></div>
      <p>This is a <a href="#test">test</a> of rich text</p>
    </div>
    <button id="go">Go</button>
    <script>
      let dir = 'forward';
      let line = 0;
      document.getElementById('go').addEventListener('click', function() {
        let sel = getSelection();
        sel.modify('move', dir, 'line');
        if (dir == 'forward') {
          line++;
        } else {
          line--;
        }

        if (line == 0) {
          dir = 'forward';
        }
        if (line == 2) {
          dir = 'backward';
        }
      }, true);
    </script>
  `,
      function(root) {
        const input = root.find({role: RoleType.TEXT_FIELD});
        const go = root.find({role: RoleType.BUTTON});
        const moveByLine = go.doDefault.bind(go);
        this.listenOnce(input, 'focus', function() {
          mockFeedback.call(moveByLine)
              .expectSpeech('\n')
              .expectBraille('\n')
              .call(moveByLine)
              .expectSpeech('This is a ', 'test', 'Link', ' of rich text')
              .expectBraille('This is a test of rich text')
              .call(moveByLine)
              .expectSpeech('\n')
              .expectBraille('\n')
              .call(moveByLine)
              .expectSpeech('hello', 'Heading 2')
              .expectBraille('hello h2 mled')
              .replay();
        });
        input.focus();
      });
});

TEST_F('ChromeVoxEditingTest', 'RichTextMoveByCharacter', function() {
  // Turn on rich text output settings.
  localStorage['announceRichTextAttributes'] = 'true';

  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(
      `
    <div role="textbox" contenteditable>This <b>is</b> a test.</div>
    <button id="go">Go</button>

    <script>
      let dir = 'forward';
      let char = 0;
      document.getElementById('go').addEventListener('click', function() {
        let sel = getSelection();
        sel.modify('move', dir, 'character');
        if (dir == 'forward') {
          char++;
        } else {
          char--;
        }

        if (char == 0) {
          dir = 'forward';
        }
        if (line == 16) {
          dir = 'backward';
        }
      }, true);
    </script>
  `,
      function(root) {
        const input = root.find({role: RoleType.TEXT_FIELD});
        const go = root.find({role: RoleType.BUTTON});
        const moveByChar = go.doDefault.bind(go);
        const lineText = 'This is a test. mled';

        this.listenOnce(input, 'focus', function() {
          mockFeedback.call(moveByChar)
              .expectSpeech('h')
              .expectBraille(lineText, {startIndex: 1, endIndex: 1})
              .call(moveByChar)
              .expectSpeech('i')
              .expectBraille(lineText, {startIndex: 2, endIndex: 2})
              .call(moveByChar)
              .expectSpeech('s')
              .expectBraille(lineText, {startIndex: 3, endIndex: 3})
              .call(moveByChar)
              .expectSpeech(' ')
              .expectBraille(lineText, {startIndex: 4, endIndex: 4})

              .call(moveByChar)
              .expectSpeech('i')
              .expectSpeech('Bold')
              .expectBraille(lineText, {startIndex: 5, endIndex: 5})

              .call(moveByChar)
              .expectSpeech('s')
              .expectBraille(lineText, {startIndex: 6, endIndex: 6})

              .call(moveByChar)
              .expectSpeech(' ')
              .expectSpeech('Not bold')
              .expectBraille(lineText, {startIndex: 7, endIndex: 7})

              .call(moveByChar)
              .expectSpeech('a')
              .expectBraille(lineText, {startIndex: 8, endIndex: 8})

              .call(moveByChar)
              .expectSpeech(' ')
              .expectBraille(lineText, {startIndex: 9, endIndex: 9})

              .replay();
        });
        input.focus();
      });
});

TEST_F(
    'ChromeVoxEditingTest', 'RichTextMoveByCharacterAllAttributes', function() {
      // Turn on rich text output settings.
      localStorage['announceRichTextAttributes'] = 'true';

      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(
          `
    <div role="textbox" contenteditable>
      <p style="font-size:20px; font-family:times">
        <b style="color:#ff0000">Move</b>
        <i>through</i> <u style="font-family:georgia">text</u>
        by <strike style="font-size:12px; color:#0000ff">character</strike>
        <a href="#">test</a>!
      </p>
    </div>

    <button id="go">Go</button>

    <script>
      document.getElementById('go').addEventListener('click', function() {
        let sel = getSelection();
        sel.modify('move', 'forward', 'character');
      }, true);
    </script>
  `,
          function(root) {
            const input = root.find({role: RoleType.TEXT_FIELD});
            const go = root.find({role: RoleType.BUTTON});
            const moveByChar = go.doDefault.bind(go);
            const lineText = 'Move through text by character test! mled';
            const lineOnLinkText =
                'Move through text by character test lnk ! mled';

            this.listenOnce(
                input, chrome.automation.EventType.FOCUS, function() {
                  mockFeedback.call(moveByChar)
                      .expectSpeech('o')
                      .expectSpeech('Size 20')
                      .expectSpeech('Red, 100% opacity.')
                      .expectSpeech('Bold')
                      .expectSpeech('Font Tinos')
                      .expectBraille(lineText, {startIndex: 1, endIndex: 1})
                      .call(moveByChar)
                      .expectSpeech('v')
                      .expectBraille(lineText, {startIndex: 2, endIndex: 2})
                      .call(moveByChar)
                      .expectSpeech('e')
                      .expectBraille(lineText, {startIndex: 3, endIndex: 3})
                      .call(moveByChar)
                      .expectSpeech(' ')
                      .expectSpeech('Black, 100% opacity.')
                      .expectSpeech('Not bold')
                      .expectBraille(lineText, {startIndex: 4, endIndex: 4})
                      .call(moveByChar)
                      .expectSpeech('t')
                      .expectSpeech('Italic')
                      .expectBraille(lineText, {startIndex: 5, endIndex: 5})
                      .call(moveByChar)
                      .expectSpeech('h')
                      .expectBraille(lineText, {startIndex: 6, endIndex: 6})
                      .call(moveByChar)
                      .expectSpeech('r')
                      .expectBraille(lineText, {startIndex: 7, endIndex: 7})
                      .call(moveByChar)
                      .expectSpeech('o')
                      .expectBraille(lineText, {startIndex: 8, endIndex: 8})
                      .call(moveByChar)
                      .expectSpeech('u')
                      .expectBraille(lineText, {startIndex: 9, endIndex: 9})
                      .call(moveByChar)
                      .expectSpeech('g')
                      .expectBraille(lineText, {startIndex: 10, endIndex: 10})
                      .call(moveByChar)
                      .expectSpeech('h')
                      .expectBraille(lineText, {startIndex: 11, endIndex: 11})
                      .call(moveByChar)
                      .expectSpeech(' ')
                      .expectSpeech('Not italic')
                      .expectBraille(lineText, {startIndex: 12, endIndex: 12})
                      .call(moveByChar)
                      .expectSpeech('t')
                      .expectSpeech('Underline')
                      .expectSpeech('Font Gelasio')
                      .expectBraille(lineText, {startIndex: 13, endIndex: 13})
                      .call(moveByChar)
                      .expectSpeech('e')
                      .expectBraille(lineText, {startIndex: 14, endIndex: 14})
                      .call(moveByChar)
                      .expectSpeech('x')
                      .expectBraille(lineText, {startIndex: 15, endIndex: 15})
                      .call(moveByChar)
                      .expectSpeech('t')
                      .expectBraille(lineText, {startIndex: 16, endIndex: 16})
                      .call(moveByChar)
                      .expectSpeech(' ')
                      .expectSpeech('Not underline')
                      .expectSpeech('Font Tinos')
                      .expectBraille(lineText, {startIndex: 17, endIndex: 17})
                      .call(moveByChar)
                      .expectSpeech('b')
                      .expectBraille(lineText, {startIndex: 18, endIndex: 18})
                      .call(moveByChar)
                      .expectSpeech('y')
                      .expectBraille(lineText, {startIndex: 19, endIndex: 19})
                      .call(moveByChar)
                      .expectSpeech(' ')
                      .expectBraille(lineText, {startIndex: 20, endIndex: 20})
                      .call(moveByChar)
                      .expectSpeech('c')
                      .expectSpeech('Size 12')
                      .expectSpeech('Blue, 100% opacity.')
                      .expectSpeech('Line through')
                      .expectBraille(lineText, {startIndex: 21, endIndex: 21})
                      .call(moveByChar)
                      .expectSpeech('h')
                      .expectBraille(lineText, {startIndex: 22, endIndex: 22})
                      .call(moveByChar)
                      .expectSpeech('a')
                      .expectBraille(lineText, {startIndex: 23, endIndex: 23})
                      .call(moveByChar)
                      .expectSpeech('r')
                      .expectBraille(lineText, {startIndex: 24, endIndex: 24})
                      .call(moveByChar)
                      .expectSpeech('a')
                      .expectBraille(lineText, {startIndex: 25, endIndex: 25})
                      .call(moveByChar)
                      .expectSpeech('c')
                      .expectBraille(lineText, {startIndex: 26, endIndex: 26})
                      .call(moveByChar)
                      .expectSpeech('t')
                      .expectBraille(lineText, {startIndex: 27, endIndex: 27})
                      .call(moveByChar)
                      .expectSpeech('e')
                      .expectBraille(lineText, {startIndex: 28, endIndex: 28})
                      .call(moveByChar)
                      .expectSpeech('r')
                      .expectBraille(lineText, {startIndex: 29, endIndex: 29})
                      .call(moveByChar)
                      .expectSpeech(' ')
                      .expectSpeech('Size 20')
                      .expectSpeech('Black, 100% opacity.')
                      .expectSpeech('Not line through')
                      .expectBraille(lineText, {startIndex: 30, endIndex: 30})
                      .call(moveByChar)
                      .expectSpeech('t')
                      .expectSpeech('Blue, 100% opacity.')
                      .expectSpeech('Link')
                      .expectSpeech('Underline')
                      .expectBraille(
                          lineOnLinkText, {startIndex: 31, endIndex: 31})
                      .call(moveByChar)
                      .expectSpeech('e')
                      .expectBraille(
                          lineOnLinkText, {startIndex: 32, endIndex: 32})
                      .call(moveByChar)
                      .expectSpeech('s')
                      .expectBraille(
                          lineOnLinkText, {startIndex: 33, endIndex: 33})
                      .call(moveByChar)
                      .expectSpeech('t')
                      .expectBraille(
                          lineOnLinkText, {startIndex: 34, endIndex: 34})
                      .call(moveByChar)
                      .expectSpeech('!')
                      .expectSpeech('Black, 100% opacity.')
                      .expectSpeech('Not link')
                      .expectSpeech('Not underline')
                      .expectBraille(lineText, {startIndex: 35, endIndex: 35})

                      .replay();
                });
            input.focus();
          });
    });

// Tests specifically for cursor workarounds.
TEST_F(
    'ChromeVoxEditingTest', 'RichTextMoveByCharacterNodeWorkaround',
    function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(
          `
    <div role="textbox" contenteditable>hello <b>world</b></div>
    <button id="go">Go</button>

    <script>
      document.getElementById('go').addEventListener('click', function() {
        let sel = getSelection();
        sel.modify('move', 'forward', 'character');
      }, true);
    </script>
  `,
          function(root) {
            const input = root.find({role: RoleType.TEXT_FIELD});
            const go = root.find({role: RoleType.BUTTON});
            const moveByChar = go.doDefault.bind(go);
            const lineText = 'hello world mled';

            this.listenOnce(input, 'focus', function() {
              mockFeedback.call(moveByChar)
                  .expectSpeech('e')
                  .expectBraille(lineText, {startIndex: 1, endIndex: 1})
                  .call(moveByChar)
                  .expectSpeech('l')
                  .expectBraille(lineText, {startIndex: 2, endIndex: 2})
                  .call(moveByChar)
                  .expectSpeech('l')
                  .expectBraille(lineText, {startIndex: 3, endIndex: 3})
                  .call(moveByChar)
                  .expectSpeech('o')
                  .expectBraille(lineText, {startIndex: 4, endIndex: 4})
                  .call(moveByChar)
                  .expectSpeech(' ')
                  .expectBraille(lineText, {startIndex: 5, endIndex: 5})
                  .call(moveByChar)
                  .expectSpeech('w')
                  .expectBraille(lineText, {startIndex: 6, endIndex: 6})
                  .replay();
            });
            input.focus();
          });
    });

TEST_F('ChromeVoxEditingTest', 'RichTextMoveByCharacterEndOfLine', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(
      `
    <div role="textbox" contenteditable>Test</div>
    <button id="go">Go</button>

    <script>
      document.getElementById('go').addEventListener('click', function() {
        let sel = getSelection();
        sel.modify('move', 'forward', 'character');
      }, true);
    </script>
  `,
      function(root) {
        const input = root.find({role: RoleType.TEXT_FIELD});
        const go = root.find({role: RoleType.BUTTON});
        const moveByChar = go.doDefault.bind(go);
        const lineText = 'Test mled';

        this.listenOnce(input, 'focus', function() {
          mockFeedback.call(moveByChar)
              .expectSpeech('e')
              .expectBraille(lineText, {startIndex: 1, endIndex: 1})
              .call(moveByChar)
              .expectSpeech('s')
              .expectBraille(lineText, {startIndex: 2, endIndex: 2})
              .call(moveByChar)
              .expectSpeech('t')
              .expectBraille(lineText, {startIndex: 3, endIndex: 3})
              .call(moveByChar)
              .expectSpeech('\n')
              .expectBraille(lineText, {startIndex: 4, endIndex: 4})

              .replay();
        });
        input.focus();
      });
});

TEST_F('ChromeVoxEditingTest', 'RichTextLinkOutput', function() {
  // Turn on rich text output settings.
  localStorage['announceRichTextAttributes'] = 'true';

  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(
      `
    <div role="textbox" contenteditable>a <a href="#">test</a></div>
    <button id="go">Go</button>
    <script>
      document.getElementById('go').addEventListener('click', function() {
        let sel = getSelection();
        sel.modify('move', 'forward', 'character');
      }, true);
    </script>
  `,
      function(root) {
        const input = root.find({role: RoleType.TEXT_FIELD});
        const go = root.find({role: RoleType.BUTTON});
        const moveByChar = go.doDefault.bind(go);
        const lineText = 'a test mled';
        const lineOnLinkText = 'a test lnk mled';

        this.listenOnce(input, 'focus', function() {
          mockFeedback.call(moveByChar)
              .expectSpeech(' ')
              .expectBraille(lineText, {startIndex: 1, endIndex: 1})
              .call(moveByChar)
              .expectSpeech('t')
              .expectSpeech('Blue, 100% opacity.')
              .expectSpeech('Link')
              .expectSpeech('Underline')
              .expectBraille(lineOnLinkText, {startIndex: 2, endIndex: 2})
              .call(moveByChar)
              .expectSpeech('e')
              .expectBraille(lineOnLinkText, {startIndex: 3, endIndex: 3})
              .call(moveByChar)
              .expectSpeech('s')
              .expectBraille(lineOnLinkText, {startIndex: 4, endIndex: 4})
              .call(moveByChar)
              .expectSpeech('t')
              .expectBraille(lineOnLinkText, {startIndex: 5, endIndex: 5})

              .replay();
        });
        input.focus();
      });
});

TEST_F('ChromeVoxEditingTest', 'RichTextExtendByCharacter', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(
      `
    <div role="textbox" contenteditable>Te<br>st</div>
    <button id="go">Go</button>

    <script>
      document.getElementById('go').addEventListener('click', function() {
        let sel = getSelection();
        sel.modify('extend', 'forward', 'character');
      }, true);
    </script>
  `,
      function(root) {
        const input = root.find({role: RoleType.TEXT_FIELD});
        const go = root.find({role: RoleType.BUTTON});
        const moveByChar = go.doDefault.bind(go);

        this.listenOnce(input, 'focus', function() {
          mockFeedback.call(moveByChar)
              .expectSpeech('T', 'selected')
              .call(moveByChar)
              .expectSpeech('e', 'added to selection')
              .call(moveByChar)
              .expectSpeech('selected')
              .call(moveByChar)
              // This gets described by the line logic in EditableLine.
              .expectSpeech('s', 'selected')
              .call(moveByChar)
              .expectSpeech('t', 'added to selection')

              .replay();
        });
        input.focus();
      });
});

TEST_F('ChromeVoxEditingTest', 'RichTextImageByCharacter', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(
      `
    <p contenteditable>
      <img alt="dog"> is a <img alt="cat"> test
    </p>
    <button id="go">Go</button>
    <script>
      let dir = 'forward';
      let moveCount = 0;
      document.getElementById('go').addEventListener('click', function() {
        moveCount++;
        if (moveCount == 9) {
          dir = 'backward';
        }

        let sel = getSelection();

        sel.modify('move', dir, 'character');
      }, true);
    </script>
  `,
      function(root) {
        const input = root.find({role: RoleType.PARAGRAPH});
        const go = root.find({role: RoleType.BUTTON});
        const moveByChar = go.doDefault.bind(go);

        this.listenOnce(input, 'focus', function() {
          const lineText = 'dog is a cat test mled';
          const lineOnCatText = 'dog is a cat img test mled';

          // This is initial output from focusing the contenteditable (which has
          // no role).
          mockFeedback.expectSpeech(
              'dog', 'Image', ' is a ', 'cat', 'Image', ' test');
          mockFeedback.expectBraille('dog img is a cat img test');

          const moves = [
            {speech: [' '], braille: [lineText, {startIndex: 3, endIndex: 3}]},
            {speech: ['i'], braille: [lineText, {startIndex: 4, endIndex: 4}]},
            {speech: ['s'], braille: [lineText, {startIndex: 5, endIndex: 5}]},
            {speech: [' '], braille: [lineText, {startIndex: 6, endIndex: 6}]},
            {speech: ['a'], braille: [lineText, {startIndex: 7, endIndex: 7}]},
            {speech: [' '], braille: [lineText, {startIndex: 8, endIndex: 8}]},
            {
              speech: ['cat', 'Image'],
              braille: [lineOnCatText, {startIndex: 9, endIndex: 9}]
            },
            {speech: [' '], braille: [lineText, {startIndex: 12, endIndex: 12}]}
          ];

          for (const item of moves) {
            mockFeedback.call(moveByChar);
            mockFeedback.expectSpeech.apply(mockFeedback, item.speech);
            mockFeedback.expectBraille.apply(mockFeedback, item.braille);
          }

          const backMoves = moves.reverse();
          backMoves.shift();
          for (const backItem of backMoves) {
            mockFeedback.call(moveByChar);
            mockFeedback.expectSpeech.apply(mockFeedback, backItem.speech);
            mockFeedback.expectBraille.apply(mockFeedback, backItem.braille);
          }

          mockFeedback.replay();
        });
        input.focus();
      });
});

TEST_F('ChromeVoxEditingTest', 'RichTextSelectByLine', function() {
  const mockFeedback = this.createMockFeedback();
  // Use digit strings like "11111" and "22222" because the character widths
  // of digits are always the same. This means the test can move down one line
  // middle of "11111" and reliably hit a given character position in "22222",
  // regardless of font configuration. https://crbug.com/898213
  this.runWithLoadedTree(
      `
    <div>
      <button id="go">Go</button>
    </div>
    <p contenteditable>
      11111 line<br>
      22222 line<br>
      33333 line<br>
    </p>
    <script>
      let commands = [
        ['extend', 'forward', 'character'],
        ['extend', 'forward', 'character'],

        ['extend', 'forward', 'line'],
        ['extend', 'forward', 'line'],

        ['extend', 'backward', 'line'],
        ['extend', 'backward', 'line'],

        ['extend', 'forward', 'documentBoundary'],

        ['move', 'forward', 'character'],
        ['move', 'backward', 'character'],
        ['move', 'backward', 'character'],

        ['extend', 'backward', 'line'],
        ['extend', 'backward', 'line'],

        ['extend', 'forward', 'line'],
      ];
      document.getElementById('go').addEventListener('click', function() {
        let sel = getSelection();
        sel.modify.apply(sel, commands.shift());
      }, true);
    </script>
  `,
      function(root) {
        const input = root.find({role: RoleType.PARAGRAPH});
        const go = root.find({role: RoleType.BUTTON});
        const move = go.doDefault.bind(go);

        this.listenOnce(input, 'focus', function() {
          // By character.
          mockFeedback.call(move)
              .expectSpeech('1', 'selected')
              .expectBraille('11111 line\nmled', {startIndex: 0, endIndex: 1})
              .call(move)
              .expectSpeech('1', 'added to selection')
              .expectBraille('11111 line\nmled', {startIndex: 0, endIndex: 2})

              // Forward selection.

              // Growing.

              // By line (notice the partial selections from the first and
              // second lines).
              .call(move)
              .expectSpeech('111 line \n 22', 'selected')
              .expectBraille('22222 line\n', {startIndex: 0, endIndex: 2})

              .call(move)
              .expectSpeech('222 line \n 33', 'selected')
              .expectBraille('33333 line\n', {startIndex: 0, endIndex: 2})

              // Shrinking.
              .call(move)
              .expectSpeech('222 line \n 33', 'unselected')
              .expectBraille('22222 line\n', {startIndex: 0, endIndex: 2})

              .call(move)
              .expectSpeech('11', 'selected')
              .expectBraille('11111 line\nmled', {startIndex: 0, endIndex: 2})

              // Document boundary.
              .call(move)
              .expectSpeech(
                  '111 line \n 22222 line \n 33333 line \n \n', 'selected')
              .expectBraille('33333 line\n', {startIndex: 0, endIndex: 10})

              // The script repositions the caret to the 'n' of the third line.
              .call(move)
              .expectSpeech('33333 line')
              .expectBraille('33333 line\n', {startIndex: 10, endIndex: 10})
              .call(move)
              .expectSpeech('e')
              .expectBraille('33333 line\n', {startIndex: 9, endIndex: 9})
              .call(move)
              .expectSpeech('n')
              .expectBraille('33333 line\n', {startIndex: 8, endIndex: 8})

              // Backward selection.

              // Growing.
              .call(move)
              .expectSpeech('ne \n 33333 li', 'selected')
              .expectBraille('22222 line\n', {startIndex: 8, endIndex: 11})

              .call(move)
              .expectSpeech('ne \n 22222 li', 'selected')
              .expectBraille('11111 line\n', {startIndex: 8, endIndex: 11})

              // Shrinking.
              .call(move)
              .expectSpeech('ne \n 22222 li', 'unselected')
              .expectBraille('22222 line\n', {startIndex: 8, endIndex: 11})

              .replay();
        });
        input.focus();
      });
});

TEST_F('ChromeVoxEditingTest', 'EditableLineOneStaticText', function() {
  this.runWithLoadedTree(
      `
    <p contenteditable style="word-spacing:100000px">this is a test</p>
  `,
      function(root) {
        const staticText = root.find({role: RoleType.STATIC_TEXT});

        let e = new editing.EditableLine(staticText, 0, staticText, 0);
        assertEquals('this ', e.text);

        assertEquals(0, e.startOffset);
        assertEquals(0, e.endOffset);
        assertEquals(0, e.localStartOffset);
        assertEquals(0, e.localEndOffset);

        assertEquals(0, e.containerStartOffset);
        assertEquals(4, e.containerEndOffset);

        e = new editing.EditableLine(staticText, 1, staticText, 1);
        assertEquals('this ', e.text);

        assertEquals(1, e.startOffset);
        assertEquals(1, e.endOffset);
        assertEquals(1, e.localStartOffset);
        assertEquals(1, e.localEndOffset);

        assertEquals(0, e.containerStartOffset);
        assertEquals(4, e.containerEndOffset);

        e = new editing.EditableLine(staticText, 5, staticText, 5);
        assertEquals('is ', e.text);

        assertEquals(0, e.startOffset);
        assertEquals(0, e.endOffset);
        assertEquals(5, e.localStartOffset);
        assertEquals(5, e.localEndOffset);

        assertEquals(0, e.containerStartOffset);
        assertEquals(2, e.containerEndOffset);

        e = new editing.EditableLine(staticText, 7, staticText, 7);
        assertEquals('is ', e.text);

        assertEquals(2, e.startOffset);
        assertEquals(2, e.endOffset);
        assertEquals(7, e.localStartOffset);
        assertEquals(7, e.localEndOffset);

        assertEquals(0, e.containerStartOffset);
        assertEquals(2, e.containerEndOffset);
      });
});

TEST_F('ChromeVoxEditingTest', 'EditableLineTwoStaticTexts', function() {
  this.runWithLoadedTree(
      `
    <p contenteditable>hello <b>world</b></p>
  `,
      function(root) {
        const text = root.find({role: RoleType.STATIC_TEXT});
        const bold = text.nextSibling;

        let e = new editing.EditableLine(text, 0, text, 0);
        assertEquals('hello world', e.text);

        assertEquals(0, e.startOffset);
        assertEquals(0, e.endOffset);
        assertEquals(0, e.localStartOffset);
        assertEquals(0, e.localEndOffset);

        assertEquals(0, e.containerStartOffset);
        assertEquals(5, e.containerEndOffset);

        e = new editing.EditableLine(text, 5, text, 5);
        assertEquals('hello world', e.text);

        assertEquals(5, e.startOffset);
        assertEquals(5, e.endOffset);
        assertEquals(5, e.localStartOffset);
        assertEquals(5, e.localEndOffset);

        assertEquals(0, e.containerStartOffset);
        assertEquals(5, e.containerEndOffset);

        e = new editing.EditableLine(bold, 0, bold, 0);
        assertEquals('hello world', e.text);

        assertEquals(6, e.startOffset);
        assertEquals(6, e.endOffset);
        assertEquals(0, e.localStartOffset);
        assertEquals(0, e.localEndOffset);

        assertEquals(6, e.containerStartOffset);
        assertEquals(10, e.containerEndOffset);

        e = new editing.EditableLine(bold, 4, bold, 4);
        assertEquals('hello world', e.text);

        assertEquals(10, e.startOffset);
        assertEquals(10, e.endOffset);
        assertEquals(4, e.localStartOffset);
        assertEquals(4, e.localEndOffset);

        assertEquals(6, e.containerStartOffset);
        assertEquals(10, e.containerEndOffset);
      });
});

TEST_F('ChromeVoxEditingTest', 'EditableLineEquality', function() {
  this.runWithLoadedTree(
      `
    <div contenteditable role="textbox">
      <p style="word-spacing:100000px">this is a test</p>
      <p>hello <b>world</b></p>
    </div>
  `,
      function(root) {
        const thisIsATest =
            root.findAll({role: RoleType.PARAGRAPH})[0].firstChild;
        const hello = root.findAll({role: RoleType.PARAGRAPH})[1].firstChild;
        const world = root.findAll({role: RoleType.PARAGRAPH})[1].lastChild;

        // The same position -- sanity check.
        let e1 = new editing.EditableLine(thisIsATest, 0, thisIsATest, 0);
        assertEquals('this ', e1.text);
        assertTrue(e1.isSameLine(e1));

        // Offset into the same soft line.
        let e2 = new editing.EditableLine(thisIsATest, 1, thisIsATest, 1);
        assertTrue(e1.isSameLine(e2));

        // Boundary.
        e2 = new editing.EditableLine(thisIsATest, 4, thisIsATest, 4);
        assertTrue(e1.isSameLine(e2));

        // Offsets into different soft lines.
        e2 = new editing.EditableLine(thisIsATest, 5, thisIsATest, 5);
        assertEquals('is ', e2.text);
        assertFalse(e1.isSameLine(e2));

        // Sanity check; second soft line.
        assertTrue(e2.isSameLine(e2));

        // Different offsets into second soft line.
        e1 = new editing.EditableLine(thisIsATest, 6, thisIsATest, 6);
        assertTrue(e1.isSameLine(e2));

        // Boundary.
        e1 = new editing.EditableLine(thisIsATest, 7, thisIsATest, 7);
        assertTrue(e1.isSameLine(e2));

        // Third line.
        e1 = new editing.EditableLine(thisIsATest, 8, thisIsATest, 8);
        assertEquals('a ', e1.text);
        assertFalse(e1.isSameLine(e2));

        // Last line.
        e2 = new editing.EditableLine(thisIsATest, 10, thisIsATest, 10);
        assertEquals('test', e2.text);
        assertFalse(e1.isSameLine(e2));

        // Boundary.
        e1 = new editing.EditableLine(thisIsATest, 13, thisIsATest, 13);
        assertTrue(e1.isSameLine(e2));

        // Cross into new paragraph.
        e2 = new editing.EditableLine(hello, 0, hello, 0);
        assertEquals('hello world', e2.text);
        assertFalse(e1.isSameLine(e2));

        // On same node, with multi-static text line.
        e1 = new editing.EditableLine(hello, 1, hello, 1);
        assertTrue(e1.isSameLine(e2));

        // On same node, with multi-static text line; boundary.
        e1 = new editing.EditableLine(hello, 5, hello, 5);
        assertTrue(e1.isSameLine(e2));

        // On different node, with multi-static text line.
        e1 = new editing.EditableLine(world, 1, world, 1);
        assertTrue(e1.isSameLine(e2));

        // Another mix of lines.
        e2 = new editing.EditableLine(thisIsATest, 9, thisIsATest, 9);
        assertFalse(e1.isSameLine(e2));
      });
});

TEST_F('ChromeVoxEditingTest', 'EditableLineStrictEquality', function() {
  this.runWithLoadedTree(
      `
    <div contenteditable role="textbox">
      <p style="word-spacing:100000px">this is a test</p>
      <p>hello <b>world</b></p>
    </div>
  `,
      function(root) {
        const thisIsATest =
            root.findAll({role: RoleType.PARAGRAPH})[0].firstChild;
        const hello = root.findAll({role: RoleType.PARAGRAPH})[1].firstChild;
        const world = root.findAll({role: RoleType.PARAGRAPH})[1].lastChild;

        // The same position -- sanity check.
        let e1 = new editing.EditableLine(thisIsATest, 0, thisIsATest, 0);
        assertEquals('this ', e1.text);
        assertTrue(e1.isSameLineAndSelection(e1));

        // Offset into the same soft line.
        let e2 = new editing.EditableLine(thisIsATest, 1, thisIsATest, 1);
        assertFalse(e1.isSameLineAndSelection(e2));

        // Boundary.
        e2 = new editing.EditableLine(thisIsATest, 4, thisIsATest, 4);
        assertFalse(e1.isSameLineAndSelection(e2));

        // Offsets into different soft lines.
        e2 = new editing.EditableLine(thisIsATest, 5, thisIsATest, 5);
        assertEquals('is ', e2.text);
        assertFalse(e1.isSameLineAndSelection(e2));

        // Sanity check; second soft line.
        assertTrue(e2.isSameLineAndSelection(e2));

        // Different offsets into second soft line.
        e1 = new editing.EditableLine(thisIsATest, 6, thisIsATest, 6);
        assertFalse(e1.isSameLineAndSelection(e2));

        // Boundary.
        e1 = new editing.EditableLine(thisIsATest, 7, thisIsATest, 7);
        assertFalse(e1.isSameLineAndSelection(e2));

        // Cross into new paragraph.
        e2 = new editing.EditableLine(hello, 0, hello, 0);
        assertEquals('hello world', e2.text);
        assertFalse(e1.isSameLineAndSelection(e2));

        // On same node, with multi-static text line.
        e1 = new editing.EditableLine(hello, 1, hello, 1);
        assertFalse(e1.isSameLineAndSelection(e2));

        // On same node, with multi-static text line; boundary.
        e1 = new editing.EditableLine(hello, 5, hello, 5);
        assertFalse(e1.isSameLineAndSelection(e2));
      });
});

TEST_F('ChromeVoxEditingTest', 'EditableLineBaseLineAnchorOrFocus', function() {
  this.runWithLoadedTree(
      `
    <div contenteditable role="textbox">
      <p style="word-spacing:100000px">this is a test</p>
      <p>hello <b>world</b></p>
    </div>
  `,
      function(root) {
        const thisIsATest =
            root.findAll({role: RoleType.PARAGRAPH})[0].firstChild;
        const hello = root.findAll({role: RoleType.PARAGRAPH})[1].firstChild;
        const world = root.findAll({role: RoleType.PARAGRAPH})[1].lastChild;

        // The same position -- sanity check.
        let e1 = new editing.EditableLine(thisIsATest, 0, thisIsATest, 0, true);
        assertEquals('this ', e1.text);

        // Offsets into different soft lines; base on focus (default).
        e1 = new editing.EditableLine(thisIsATest, 0, thisIsATest, 6);
        assertEquals('is ', e1.text);
        // Notice that the offset is truncated at the beginning of the line.
        assertEquals(0, e1.startOffset);
        // Notice that the end offset is properly retained.
        assertEquals(1, e1.endOffset);

        // Offsets into different soft lines; base on anchor.
        e1 = new editing.EditableLine(thisIsATest, 0, thisIsATest, 6, true);
        assertEquals('this ', e1.text);
        assertEquals(0, e1.startOffset);
        // Notice            that the end offset is truncated up to the end of
        // line.
        assertEquals(5, e1.endOffset);

        // Across paragraph selection with base line on focus.
        e1 = new editing.EditableLine(thisIsATest, 5, hello, 2);
        assertEquals('hello world', e1.text);
        assertEquals(0, e1.startOffset);
        assertEquals(2, e1.endOffset);

        // Across paragraph selection with base line on anchor.
        e1 = new editing.EditableLine(thisIsATest, 5, hello, 2, true);
        assertEquals('is ', e1.text);
        assertEquals(0, e1.startOffset);
        assertEquals(3, e1.endOffset);
      });
});

TEST_F('ChromeVoxEditingTest', 'IsValidLine', function() {
  this.runWithLoadedTree(
      `
    <div contenteditable role="textbox">
      <p style="word-spacing:100000px">this is a test</p>
      <p>end</p>
    </div>
  `,
      function(root) {
        // Each word is on its own line, but parented by a static text.
        const [text, endText] = root.findAll({role: RoleType.STATIC_TEXT});

        // The EditableLine object automatically adjusts to surround the line no
        // matter what the input is.
        const line = new editing.EditableLine(text, 0, text, 0);
        assertTrue(line.isValidLine());

        // During the course of editing operations, this line may become
        // invalidted. For example, if a user starts typing into the line, the
        // bounding nodes might change.
        // Simulate that here by modifying private state.

        // This puts the line at offset 8 (|this is a|).
        line.localLineStartContainerOffset_ = 0;
        line.localLineEndContainerOffset_ = 8;
        assertFalse(line.isValidLine());

        // This puts us in the first line.
        line.localLineStartContainerOffset_ = 0;
        line.localLineEndContainerOffset_ = 4;
        assertTrue(line.isValidLine());

        // This is still fine (for our purposes) because the line is still
        // intact.
        line.localLineStartContainerOffset_ = 0;
        line.localLineEndContainerOffset_ = 2;
        assertTrue(line.isValidLine());

        // The line has changed. The end has been moved for some reason.
        line.lineEndContainer_ = endText;
        assertFalse(line.isValidLine());
      });
});

TEST_F('ChromeVoxEditingTest', 'TelTrimsWhitespace', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(
      `
    <div id="go"></div>
    <input id="input" type="tel"></input>
    <script>
      let data = [
        '6               ',
        '60              ',
        '601             ',
        '60              '
      ];
      let go = document.getElementById('go');
      let input = document.getElementById('input');
      let index = 0;
      go.addEventListener('click', function() {
        input.value = data[index];
        index++;
        input.selectionStart = index;
        input.selectionEnd = index;
      }, true);
    </script>
  `,
      function(root) {
        const input = root.find({role: RoleType.TEXT_FIELD});
        const go = root.find({role: RoleType.GENERIC_CONTAINER});
        const enterKey = go.doDefault.bind(go);
        this.listenOnce(input, 'focus', function() {
          mockFeedback.call(enterKey)
              .expectSpeech('6')
              .call(enterKey)
              .expectSpeech('0')
              .call(enterKey)
              .expectSpeech('1')

              // Deletion.
              .call(enterKey)
              .expectSpeech('1')
              .replay();
        });
        input.focus();
      });
});

TEST_F('ChromeVoxEditingTest', 'BackwardWordDelete', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(
      `
    <div
        style='max-width: 5px; overflow-wrap: normal'
        contenteditable
        role="textbox">
      this is a test
    </div>
  `,
      function(root) {
        const input = root.find({role: RoleType.TEXT_FIELD});
        this.listenOnce(input, 'focus', function() {
          mockFeedback.call(this.press(KeyCode.END, {ctrl: true}))
              .expectSpeech('test')
              .call(this.press(KeyCode.BACK, {ctrl: true}))
              .expectSpeech('test, deleted')
              .expectBraille('a\u00a0', {startIndex: 2, endIndex: 2})
              .call(this.press(KeyCode.BACK, {ctrl: true}))
              .expectSpeech('a , deleted')
              .expectBraille('is\u00a0', {startIndex: 3, endIndex: 3})
              .call(this.press(KeyCode.BACK, {ctrl: true}))
              .expectSpeech('is , deleted')
              .expectBraille('this\u00a0mled', {startIndex: 5, endIndex: 5})
              .call(this.press(KeyCode.BACK, {ctrl: true}))
              .expectSpeech('this , deleted')
              .expectBraille(' mled', {startIndex: 0, endIndex: 0})
              .replay();
        });
        input.focus();
      });
});

TEST_F(
    'ChromeVoxEditingTest', 'BackwardWordDeleteAcrossParagraphs', function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(
          `
    <div
        style='max-width: 5px; overflow-wrap: normal'
        contenteditable
        role="textbox">
      <p>first line</p>
      <p>second line</p>
    </div>
  `,
          function(root) {
            const input = root.find({role: RoleType.TEXT_FIELD});
            this.listenOnce(input, 'focus', function() {
              mockFeedback.call(this.press(KeyCode.END, {ctrl: true}))
                  .expectSpeech('line')
                  .call(this.press(KeyCode.BACK, {ctrl: true}))
                  .expectSpeech('line, deleted')
                  .call(this.press(KeyCode.BACK, {ctrl: true}))
                  .expectSpeech('second , deleted')
                  .call(this.press(KeyCode.BACK, {ctrl: true}))
                  .expectSpeech('line')
                  .call(this.press(KeyCode.BACK, {ctrl: true}))
                  .expectSpeech('line, deleted')
                  .call(this.press(KeyCode.BACK, {ctrl: true}))
                  .expectSpeech('first , deleted')
                  .replay();
            });
            input.focus();
          });
    });

TEST_F('ChromeVoxEditingTest', 'GrammarErrors', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(
      `
    <div contenteditable="true" role="textbox">
      This <span aria-invalid="grammar">are</span> a test
    </div>
    <button id="go">Go</button>

    <script>
      document.getElementById('go').addEventListener('click', function() {
        let sel = getSelection();
        sel.modify('move', 'forward', 'character');
      }, true);
    </script>
  `,
      function(root) {
        const input = root.find({role: RoleType.TEXT_FIELD});
        const go = root.find({role: RoleType.BUTTON});
        const moveByChar = go.doDefault.bind(go);

        this.listenOnce(input, 'focus', function() {
          mockFeedback.call(moveByChar)
              .expectSpeech('h')
              .call(moveByChar)
              .expectSpeech('i')
              .call(moveByChar)
              .expectSpeech('s')
              .call(moveByChar)
              .expectSpeech(' ')

              .call(moveByChar)
              .expectSpeech('a', 'Grammar error')
              .call(moveByChar)
              .expectSpeech('r')
              .call(moveByChar)
              .expectSpeech('e')
              .call(moveByChar)
              .expectSpeech(' ', 'Leaving grammar error')

              .replay();
        });
        input.focus();
      });
});

// Flaky test, crbug.com/1098642.
TEST_F(
    'ChromeVoxEditingTest', 'DISABLED_CharacterTypedAfterNewLine', function() {
      const mockFeedback = this.createMockFeedback();
      this.runWithLoadedTree(
          `
    <p>start</p>
    <div contenteditable role="textbox">
      <p>hello</p>
    </div>
  `,
          function(root) {
            const input = root.find({role: RoleType.TEXT_FIELD});
            this.listenOnce(input, 'focus', function() {
              mockFeedback.call(this.press(KeyCode.END, {ctrl: true}))
                  .expectSpeech('hello')
                  .call(this.press(KeyCode.RETURN))
                  .expectSpeech('\n')
                  .call(this.press(KeyCode.A))
                  .expectSpeech('a')
                  .replay();
            });
            input.focus();
          });
    });

TEST_F('ChromeVoxEditingTest', 'SelectAll', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(
      `
    <div contenteditable role="textbox">
      <p>first line</p>
      <p>second line</p>
      <p>third line</p>
    </div>
  `,
      function(root) {
        const input = root.find({role: RoleType.TEXT_FIELD});
        this.listenOnce(input, 'focus', function() {
          mockFeedback.call(this.press(KeyCode.END, {ctrl: true}))
              .expectSpeech('third line')
              .call(this.press(KeyCode.A, {ctrl: true}))
              .expectSpeech('first line second line third line', 'selected')
              .call(this.press(KeyCode.UP))
              .expectSpeech('second line')
              .call(this.press(KeyCode.A, {ctrl: true}))
              .expectSpeech('first line second line third line', 'selected')
              .call(this.press(KeyCode.HOME, {ctrl: true}))
              .expectSpeech('first line')
              .call(this.press(KeyCode.A, {ctrl: true}))
              .expectSpeech('first line second line third line', 'selected')
              .replay();
        });
        input.focus();
      });
});

TEST_F('ChromeVoxEditingTest', 'TextAreaBrailleEmptyLine', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree('<textarea></textarea>', function(root) {
    const textarea = root.find({role: RoleType.TEXT_FIELD});
    this.listenOnce(textarea, 'focus', function() {
      this.listenOnce(textarea, 'valueInTextFieldChanged', function() {
        mockFeedback.call(this.press(KeyCode.UP)).expectBraille('\n');
        mockFeedback.call(this.press(KeyCode.UP)).expectBraille('two\n');
        mockFeedback.call(this.press(KeyCode.UP)).expectBraille('one\n');
        mockFeedback.call(this.press(KeyCode.UP)).expectBraille('\n');
        mockFeedback.call(this.press(KeyCode.UP))
            .expectBraille('test\nmled')
            .replay();
      });
    });
    textarea.focus();
    textarea.setValue('test\n\none\ntwo\n\nthree');
  });
});

TEST_F('ChromeVoxEditingTest', 'MoveByCharacterIntent', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(
      `
    <div contenteditable role="textbox">
      <p>123</p>
      <p>456</p>
    </div>
  `,
      function(root) {
        const input = root.find({role: RoleType.TEXT_FIELD});
        this.listenOnce(input, 'focus', function() {
          mockFeedback.call(this.press(KeyCode.RIGHT))
              .expectSpeech('2')
              .call(this.press(KeyCode.RIGHT))
              .expectSpeech('3')
              .call(this.press(KeyCode.RIGHT))
              .expectSpeech('\n')
              .call(this.press(KeyCode.RIGHT))
              .expectSpeech('4')
              .call(this.press(KeyCode.LEFT))
              .expectSpeech('\n')
              .call(this.press(KeyCode.LEFT))
              .expectSpeech('3')
              .replay();
        });
        input.focus();
      });
});

TEST_F('ChromeVoxEditingTest', 'MoveByLineIntent', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(
      `
    <div contenteditable role="textbox">
      <p>123</p>
      <p>456</p>
      <p>789</p>
    </div>
  `,
      function(root) {
        const input = root.find({role: RoleType.TEXT_FIELD});
        this.listenOnce(input, 'focus', function() {
          mockFeedback.call(this.press(KeyCode.DOWN))
              .expectSpeech('456')
              .call(this.press(KeyCode.DOWN))
              .expectSpeech('789')
              .call(this.press(KeyCode.UP))
              .expectSpeech('456')
              .call(this.press(KeyCode.UP))
              .expectSpeech('123')
              .replay();
        });
        input.focus();
      });
});

TEST_F('ChromeVoxEditingTest', 'SelectAllBareTextContent', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(
      `
    <div contenteditable role="textbox">unread</div>
  `,
      function(root) {
        const input = root.find({role: RoleType.TEXT_FIELD});
        this.listenOnce(input, 'focus', function() {
          mockFeedback.call(this.press(KeyCode.END, {ctrl: true}))
              .expectSpeech('unread')
              .call(this.press(KeyCode.A, {ctrl: true}))
              .expectSpeech('unread', 'selected')
              .replay();
        });
        input.focus();
      });
});

TEST_F('ChromeVoxEditingTest', 'NonBreakingSpaceNewLine', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(
      `
    <div contenteditable role="textbox">&nbsp</div>
  `,
      function(root) {
        const input = root.find({role: RoleType.TEXT_FIELD});
        this.listenOnce(input, 'focus', function() {
          mockFeedback
              .call(() => {
                const node = root.find({role: RoleType.INLINE_TEXT_BOX});
                const line = new editing.EditableLine(node, 0, node, 0);
                const prev =
                    new editing.EditableLine(node.root, 1, node.root, 1);
                const editableHandler = DesktopAutomationHandler.instance
                                            .textEditHandler_.editableText_;
                editableHandler.handleSpeech_(
                    line, prev, line, line, prev, prev, true, []);
              })
              .expectSpeech('\n')
              .replay();
        });
        input.focus();
      });
});

TEST_F('ChromeVoxEditingTest', 'InputEvents', function() {
  const site = `<input type="text"></input>`;
  this.runWithLoadedTree(site, async function(root) {
    const input = root.find({role: RoleType.TEXT_FIELD});
    input.focus();
    await new Promise(resolve => {
      this.listenOnce(input, 'focus', resolve);
    });

    // EventType.TEXT_SELECTION_CHANGED fires on focus as well.
    //
    // TODO(nektar): Deprecate and remove TEXT_SELECTION_CHANGED.
    event = await this.waitForEditableEvent();
    assertEquals(EventType.TEXT_SELECTION_CHANGED, event.type);
    assertEquals(input, event.target);
    assertEquals('', input.value);

    this.press(KeyCode.A)();

    event = await this.waitForEditableEvent();
    assertEquals(EventType.VALUE_IN_TEXT_FIELD_CHANGED, event.type);
    assertEquals(input, event.target);
    assertEquals('a', input.value);

    // We deliberately used EventType.TEXT_SELECTION_CHANGED instead of
    // EventType.DOCUMENT_SELECTION_CHANGED for text fields.
    event = await this.waitForEditableEvent();
    assertEquals(EventType.TEXT_SELECTION_CHANGED, event.type);
    assertEquals(input, event.target);
    assertEquals('a', input.value);

    this.press(KeyCode.B)();

    event = await this.waitForEditableEvent();
    assertEquals(EventType.VALUE_IN_TEXT_FIELD_CHANGED, event.type);
    assertEquals(input, event.target);
    assertEquals('ab', input.value);

    event = await this.waitForEditableEvent();
    assertEquals(EventType.TEXT_SELECTION_CHANGED, event.type);
    assertEquals(input, event.target);
    assertEquals('ab', input.value);
  });
});

TEST_F('ChromeVoxEditingTest', 'TextAreaEvents', function() {
  const site = `<textarea></textarea>`;
  this.runWithLoadedTree(site, async function(root) {
    const textArea = root.find({role: RoleType.TEXT_FIELD});
    textArea.focus();
    await new Promise(resolve => {
      this.listenOnce(textArea, 'focus', resolve);
    });

    let event = await this.waitForEditableEvent();
    assertEquals(EventType.DOCUMENT_SELECTION_CHANGED, event.type);
    assertEquals(textArea, event.target);
    assertEquals('', textArea.value);

    this.press(KeyCode.A)();

    event = await this.waitForEditableEvent();
    assertEquals(EventType.DOCUMENT_SELECTION_CHANGED, event.type);
    assertEquals(textArea, event.target);
    assertEquals('a', textArea.value);

    this.press(KeyCode.B)();

    event = await this.waitForEditableEvent();
    assertEquals(EventType.DOCUMENT_SELECTION_CHANGED, event.type);
    assertEquals(textArea, event.target);
    assertEquals('ab', textArea.value);
  });
});

TEST_F('ChromeVoxEditingTest', 'ContentEditableEvents', function() {
  const site = `<div role="textbox" contenteditable></div>`;
  this.runWithLoadedTree(site, async function(root) {
    const contentEditable = root.find({role: RoleType.TEXT_FIELD});
    contentEditable.focus();
    await new Promise(resolve => {
      this.listenOnce(contentEditable, 'focus', resolve);
    });

    let event = await this.waitForEditableEvent();
    assertEquals(EventType.DOCUMENT_SELECTION_CHANGED, event.type);
    assertEquals(contentEditable, event.target);
    assertEquals('', contentEditable.value);

    this.press(KeyCode.A)();

    event = await this.waitForEditableEvent();
    assertEquals(EventType.DOCUMENT_SELECTION_CHANGED, event.type);
    assertEquals(contentEditable, event.target);
    assertEquals('a', contentEditable.value);

    this.press(KeyCode.B)();

    event = await this.waitForEditableEvent();
    assertEquals(EventType.DOCUMENT_SELECTION_CHANGED, event.type);
    assertEquals(contentEditable, event.target);
    assertEquals('ab', contentEditable.value);
  });
});

TEST_F('ChromeVoxEditingTest', 'MarkedContent', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div contenteditable role="textbox">
      <p>Start</p>
      <span>This is </span><span role="mark">my</span><span> text.</span>
      <br>
      <span>This is </span><span role="mark"
          aria-roledescription="Comment">your</span><span> text.</span>
      <br>
      <span>This is </span><span role="suggestion"><span
          role="insertion">their</span></span><span> text.</span>
      <br>
      <span>This is </span><span role="suggestion"><span
          role="deletion">everyone's</span></span><span> text.</span>
    </div>
  `;
  this.runWithLoadedTree(site, function(root) {
    const input = root.find({role: RoleType.TEXT_FIELD});
    this.listenOnce(input, 'focus', function() {
      mockFeedback.call(this.press(KeyCode.DOWN))
          .expectSpeech(
              'This is ', 'my', 'Marked content', ' text.',
              'Exited Marked content.')
          .call(this.press(KeyCode.DOWN))
          .expectSpeech(
              'This is ', 'your', 'Comment', ' text.', 'Exited Comment.')
          .call(this.press(KeyCode.DOWN))
          .expectSpeech(
              'This is ', 'Suggestion', 'Insertion', 'their', ' text.',
              'Exited Insertion.', 'Exited Suggestion.')
          .call(this.press(KeyCode.DOWN))
          .expectSpeech(
              'This is ', 'Suggestion', 'Deletion', `everyone's`, ' text.',
              'Exited Deletion.', 'Exited Suggestion.')
          .replay();
    });
    input.focus();
  });
});

TEST_F('ChromeVoxEditingTest', 'NestedInsertionDeletion', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div contenteditable role="textbox">
      <p>Start</p>
      <span>I </span>
      <span role="suggestion" aria-description="Username">
        <span role="insertion">was</span>
        <span role="deletion">am</span></span><span> typing</span>
      <p>End</p>
    </div>
  `;
  this.runWithLoadedTree(site, function(root) {
    const input = root.find({role: RoleType.TEXT_FIELD});
    this.listenOnce(input, 'focus', function() {
      mockFeedback.call(this.press(KeyCode.DOWN))
          .expectSpeech(
              'I ', 'Suggestion', 'Username', 'Insertion', 'was',
              'Exited Insertion.', 'Deletion', 'am', ' typing',
              'Exited Deletion.', 'Exited Suggestion.')
          .call(this.press(KeyCode.DOWN))
          .expectSpeech('End')
          .replay();
    });
    input.focus();
  });
});

TEST_F('ChromeVoxEditingTest', 'Separator', function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div contenteditable="true" role="textbox">
      <p>Start</p>
      <p><span>Hello</span></p>
      <p><span role="separator">Separator content should not be read</span></p>
      <p><span>World</span></p>
    </div>
  `;
  this.runWithLoadedTree(site, function(root) {
    const input = root.find({role: RoleType.TEXT_FIELD});
    this.listenOnce(input, 'focus', function() {
      mockFeedback.call(this.press(KeyCode.DOWN))
          .expectSpeech('Hello')
          .call(this.press(KeyCode.DOWN))
          .expectNextSpeechUtteranceIsNot(
              'Separator content should not be read')
          .expectSpeech('Separator')
          .call(this.press(KeyCode.DOWN))
          .expectSpeech('World')
          .call(this.press(KeyCode.LEFT))
          .expectSpeech('Separator')
          .replay();
    });
    input.focus();
  });
});
