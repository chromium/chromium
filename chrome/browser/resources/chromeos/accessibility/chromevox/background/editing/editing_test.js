// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);

/**
 * Test fixture for editing tests.
 */
ChromeVoxEditingTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    globalThis.EventType = chrome.automation.EventType;
    globalThis.IntentCommandType = chrome.automation.IntentCommandType;
    globalThis.RoleType = chrome.automation.RoleType;
  }

  press(keyCode, modifiers) {
    return function() {
      EventGenerator.sendKeyPress(keyCode, modifiers);
    };
  }

  waitForEditableEvent() {
    return new Promise(resolve => {
      DesktopAutomationInterface.instance.textEditHandler_.onEvent = e =>
          resolve(e);
    });
  }

  async focusFirstTextField(root, opt_findParams) {
    const findParams = opt_findParams || {role: RoleType.TEXT_FIELD};
    const input = root.find(findParams);
    input.focus();
    await this.waitForEvent(input, EventType.FOCUS);
    return input;
  }

  routeBraille(mockFeedback, position) {
    BrailleCommandHandler.onBrailleKeyEvent(
        {command: BrailleKeyCommand.ROUTING, displayPosition: position},
        mockFeedback.lastMatchedBraille);
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

AX_TEST_F('ChromeVoxEditingTest', 'Focus', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(doc);
  const singleLine =
      root.find({role: RoleType.TEXT_FIELD, attributes: {name: 'singleLine'}});
  const textarea =
      root.find({role: RoleType.TEXT_FIELD, attributes: {name: 'textArea'}});
  singleLine.focus();
  mockFeedback.expectSpeech('singleLine', 'Single line field', 'Edit text')
      .expectBraille(
          'singleLine Single line field ed', {startIndex: 11, endIndex: 11})
      .call(textarea.focus.bind(textarea))
      .expectSpeech('textArea', 'Line 1\nline 2\nline 3', 'Text area')
      .expectBraille(
          'textArea Line 1\nline 2\nline 3 mled', {startIndex: 9, endIndex: 9});

  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxEditingTest', 'Multiline', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(doc);
  const textarea =
      root.find({role: RoleType.TEXT_FIELD, attributes: {name: 'textArea'}});
  textarea.focus();
  mockFeedback.expectSpeech('textArea', 'Line 1\nline 2\nline 3', 'Text area')
      .expectBraille(
          'textArea Line 1\nline 2\nline 3 mled', {startIndex: 9, endIndex: 9})
      .call(textarea.setSelection.bind(textarea, 1, 1))
      .expectSpeech('i')
      .expectBraille('Line 1\nmled', {startIndex: 1, endIndex: 1})
      .call(textarea.setSelection.bind(textarea, 7, 7))
      .expectSpeech('line 2')
      .expectBraille('line 2\n', {startIndex: 0, endIndex: 0})
      .call(textarea.setSelection.bind(textarea, 7, 13))
      .expectSpeech('line 2', 'selected')
      .expectBraille('line 2\n', {startIndex: 0, endIndex: 6});

  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxEditingTest', 'TextButNoSelectionChange', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(`
      <h1>Test doc</h1>
      <input type='text' id='input' value='text1'>
      <!-- We don't seem to get an event in js when the automation
           setSelection function is called, so poll for the actual change. -->

      <script>
        let timer;
        let input = document.getElementById('input');
        function poll(e) {
          if (input.selectionStart === 0) {
            return;
          }

          input.value = 'text2';
          clearInterval(timer);
        }
        timer = setInterval(poll, 200);
      </script>
    `);
  const input = root.find({role: RoleType.TEXT_FIELD});
  input.focus();
  mockFeedback.expectSpeech('text1', 'Edit text')
      .expectBraille('text1 ed', {startIndex: 0, endIndex: 0})
      .call(input.setSelection.bind(input, 5, 5))
      .expectBraille('text2 ed', {startIndex: 5, endIndex: 5});

  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxEditingTest', 'RichTextMoveByLine', async function() {
  // Turn on rich text output settings.
  SettingsManager.set('announceRichTextAttributes', true);

  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(`
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
        if (dir === 'forward') {
          line++;
        } else {
          line--;
        }

        if (line === 0) {
          dir = 'forward';
        }
        if (line === 2) {
          dir = 'backward';
        }
      }, true);
    </script>
  `);
  await this.focusFirstTextField(root);

  const go = root.find({role: RoleType.BUTTON});
  const moveByLine = go.doDefault.bind(go);
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
      .expectBraille('hello h2 mled');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxEditingTest', 'RichTextMoveByCharacter', async function() {
  // Turn on rich text output settings.
  SettingsManager.set('announceRichTextAttributes', true);

  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(`
    <div role="textbox" contenteditable>This <b>is</b> a test.</div>
    <button id="go">Go</button>

    <script>
      let dir = 'forward';
      let char = 0;
      document.getElementById('go').addEventListener('click', function() {
        let sel = getSelection();
        sel.modify('move', dir, 'character');
        if (dir === 'forward') {
          char++;
        } else {
          char--;
        }

        if (char === 0) {
          dir = 'forward';
        }
        if (char === 16) {
          dir = 'backward';
        }
      }, true);
    </script>
  `);
  await this.focusFirstTextField(root);

  const go = root.find({role: RoleType.BUTTON});
  const moveByChar = go.doDefault.bind(go);
  const lineText = 'This is a test. mled';

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
      .expectBraille(lineText, {startIndex: 9, endIndex: 9});

  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxEditingTest', 'RichTextMoveByCharacterAllAttributes',
    async function() {
      // Turn on rich text output settings.
      SettingsManager.set('announceRichTextAttributes', true);

      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(`
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
  `);
      await this.focusFirstTextField(root);

      const go = root.find({role: RoleType.BUTTON});
      const moveByChar = go.doDefault.bind(go);
      const lineText = 'Move through text by character test! mled';
      const lineOnLinkText = 'Move through text by character test lnk ! mled';

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
          .expectBraille(lineOnLinkText, {startIndex: 31, endIndex: 31})
          .call(moveByChar)
          .expectSpeech('e')
          .expectBraille(lineOnLinkText, {startIndex: 32, endIndex: 32})
          .call(moveByChar)
          .expectSpeech('s')
          .expectBraille(lineOnLinkText, {startIndex: 33, endIndex: 33})
          .call(moveByChar)
          .expectSpeech('t')
          .expectBraille(lineOnLinkText, {startIndex: 34, endIndex: 34})
          .call(moveByChar)
          .expectSpeech('!')
          .expectSpeech('Black, 100% opacity.')
          .expectSpeech('Not link')
          .expectSpeech('Not underline')
          .expectBraille(lineText, {startIndex: 35, endIndex: 35});

      await mockFeedback.replay();
    });

// Tests specifically for cursor workarounds.
AX_TEST_F(
    'ChromeVoxEditingTest', 'RichTextMoveByCharacterNodeWorkaround',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(`
    <div role="textbox" contenteditable>hello <b>world</b></div>
    <button id="go">Go</button>

    <script>
      document.getElementById('go').addEventListener('click', function() {
        let sel = getSelection();
        sel.modify('move', 'forward', 'character');
      }, true);
    </script>
  `);
      await this.focusFirstTextField(root);

      const go = root.find({role: RoleType.BUTTON});
      const moveByChar = go.doDefault.bind(go);
      const lineText = 'hello world mled';

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
          .expectBraille(lineText, {startIndex: 6, endIndex: 6});
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxEditingTest', 'RichTextMoveByCharacterEndOfLine',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(`
    <div role="textbox" contenteditable>Test</div>
    <button id="go">Go</button>

    <script>
      document.getElementById('go').addEventListener('click', function() {
        let sel = getSelection();
        sel.modify('move', 'forward', 'character');
      }, true);
    </script>
  `);
      await this.focusFirstTextField(root);

      const go = root.find({role: RoleType.BUTTON});
      const moveByChar = go.doDefault.bind(go);
      const lineText = 'Test mled';

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
          .expectSpeech('End of text')
          .expectBraille(lineText, {startIndex: 4, endIndex: 4});

      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxEditingTest', 'RichTextLinkOutput', async function() {
  // Turn on rich text output settings.
  SettingsManager.set('announceRichTextAttributes', true);

  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(`
    <div role="textbox" contenteditable>a <a href="#">test</a></div>
    <button id="go">Go</button>
    <script>
      document.getElementById('go').addEventListener('click', function() {
        let sel = getSelection();
        sel.modify('move', 'forward', 'character');
      }, true);
    </script>
  `);
  await this.focusFirstTextField(root);

  const go = root.find({role: RoleType.BUTTON});
  const moveByChar = go.doDefault.bind(go);
  const lineText = 'a test mled';
  const lineOnLinkText = 'a test lnk mled';

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
      .expectBraille(lineOnLinkText, {startIndex: 5, endIndex: 5});

  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxEditingTest', 'RichTextExtendByCharacter', async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(`
    <div role="textbox" contenteditable>Te<br>st</div>
    <button id="go">Go</button>

    <script>
      document.getElementById('go').addEventListener('click', function() {
        let sel = getSelection();
        sel.modify('extend', 'forward', 'character');
      }, true);
    </script>
  `);
      await this.focusFirstTextField(root);

      const go = root.find({role: RoleType.BUTTON});
      const moveByChar = go.doDefault.bind(go);

      mockFeedback.call(moveByChar)
          .expectSpeech('T', 'selected')
          .call(moveByChar)
          .expectSpeech('e', 'selected')
          .call(moveByChar)
          .expectSpeech('selected')
          .call(moveByChar)
          .expectSpeech('s', 'selected')
          .call(moveByChar)
          .expectSpeech('t', 'selected');

      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxEditingTest', 'RichTextImageByCharacter', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(`
    <p contenteditable>
      <img alt="dog"> is a <img alt="cat"> test
    </p>
    <button id="go">Go</button>
    <script>
      let dir = 'forward';
      let moveCount = 0;
      document.getElementById('go').addEventListener('click', function() {
        moveCount++;
        if (moveCount === 9) {
          dir = 'backward';
        }

        let sel = getSelection();

        sel.modify('move', dir, 'character');
      }, true);
    </script>
  `);
  await this.focusFirstTextField(root, {role: RoleType.PARAGRAPH});

  const go = root.find({role: RoleType.BUTTON});
  const moveByChar = go.doDefault.bind(go);
  const lineText = 'dog is a cat test mled';
  const lineOnCatText = 'dog is a cat img test mled';

  // This is initial output from focusing the contenteditable (which has
  // no role).
  mockFeedback.expectSpeech('dog', 'Image', ' is a ', 'cat', 'Image', ' test');
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
      braille: [lineOnCatText, {startIndex: 9, endIndex: 9}],
    },
    {speech: [' '], braille: [lineText, {startIndex: 12, endIndex: 12}]},
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

  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxEditingTest', 'RichTextSelectByLine', async function() {
  const mockFeedback = this.createMockFeedback();
  // Use digit strings like "11111" and "22222" because the character widths
  // of digits are always the same. This means the test can move down one line
  // middle of "11111" and reliably hit a given character position in "22222",
  // regardless of font configuration. https://crbug.com/898213
  const root = await this.runWithLoadedTree(`
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
  `);
  await this.focusFirstTextField(root, {role: RoleType.PARAGRAPH});

  const go = root.find({role: RoleType.BUTTON});
  const move = go.doDefault.bind(go);

  // By character.
  mockFeedback.call(move)
      .expectSpeech('1', 'selected')
      .expectBraille('11111 line\nmled', {startIndex: 0, endIndex: 1})
      .call(move)
      .expectSpeech('1', 'selected')
      .expectBraille('11111 line\nmled', {startIndex: 0, endIndex: 2})

      // Forward selection by line (notice the partial selections from the
      // first and second lines).
      .call(move)
      .expectSpeech('111 line', '22', 'selected')
      .expectBraille('22222 line\n', {startIndex: 0, endIndex: 2})

      .call(move)
      .expectSpeech('222 line', '33', 'selected')
      .expectBraille('33333 line\n', {startIndex: 0, endIndex: 2})

      // Backward selection by line.
      .call(move)
      .expectSpeech('222 line', '33', 'unselected')
      .expectBraille('22222 line\n', {startIndex: 0, endIndex: 2})

      .call(move)
      .expectSpeech('111 line', '22', 'unselected')
      .expectBraille('11111 line\nmled', {startIndex: 0, endIndex: 2})

      // Document boundary.
      .call(move)
      .expectSpeech('111 line', '22222 line', '33333 line', 'selected')
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
      .expectSpeech('ne', '33333 li', 'selected')
      .expectBraille('22222 line\n', {startIndex: 8, endIndex: 11})

      .call(move)
      .expectSpeech('ne', '22222 li', 'selected')
      .expectBraille('11111 line\n', {startIndex: 8, endIndex: 11})

      // Shrinking.
      .call(move)
      .expectSpeech('ne', '22222 li', 'unselected')
      .expectBraille('22222 line\n', {startIndex: 8, endIndex: 11});

  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxEditingTest', 'RichTextSelectComplexStructure', async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(`
    <div>
      <button id="go">Go</button>
    </div>
    <div contenteditable role=textbox>
      <h1>11111 line</h1>
      <a href=#>22222 line</a>
      <ol><li>33333 line</li></ol>
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
  `);
      await this.focusFirstTextField(root, {role: RoleType.TEXT_FIELD});

      const go = root.find({role: RoleType.BUTTON});
      const move = go.doDefault.bind(go);

      // By character.
      mockFeedback.call(move)
          .expectSpeech('1', 'Heading 1', 'selected')
          .expectBraille('11111 line h1 mled', {startIndex: 0, endIndex: 1})
          .call(move)
          .expectSpeech('1', 'Heading 1', 'selected')
          .expectBraille('11111 line h1 mled', {startIndex: 0, endIndex: 2})

          // Forward selection by line (notice the partial selections from the
          // first and second lines).
          .call(move)
          .expectSpeech('111 line', 'Heading 1', '222', 'Link', 'selected')
          .expectBraille('22222 line lnk', {startIndex: 0, endIndex: 3})

          .call(move)
          .expectSpeech('22 line', 'Link', 'selected')
          .expectBraille(
              '33333 line 1. lstitm lst +1', {startIndex: 0, endIndex: 0})

          // Shrinking.
          .call(move)
          .expectSpeech('22 line', 'Link', 'unselected')
          .expectBraille('22222 line lnk', {startIndex: 0, endIndex: 3})

          .call(move)
          .expectSpeech('111 line', 'Heading 1', '222', 'Link', 'unselected')
          .expectBraille('11111 line h1 mled', {startIndex: 0, endIndex: 2})

          // Document boundary.
          .call(move)
          .expectSpeech(
              '111 line', 'Heading 1', '22222 line', 'Link', '33333 line',
              'List item', 'selected')
          .expectBraille(
              '33333 line 1. lstitm lst +1', {startIndex: 0, endIndex: 10})

          // The script repositions the caret to the end of the last line.
          .call(move)
          .expectSpeech('End of text')
          .expectBraille(
              '33333 line 1. lstitm lst +1', {startIndex: 10, endIndex: 10})
          .call(move)
          .expectSpeech('e')
          .expectBraille(
              '33333 line 1. lstitm lst +1', {startIndex: 9, endIndex: 9})
          .call(move)
          .expectSpeech('n')
          .expectBraille(
              '33333 line 1. lstitm lst +1', {startIndex: 8, endIndex: 8})

          // Backward selection.
          // Some bugs exist in Blink where we don't get all selection events
          // in this complex structure via extending selection, so we do it
          // twice.
          .call(move)
          .call(move)
          .expectSpeech('ine', 'Link')
          .expectSpeech('33333 li', 'List item', 'selected')
          .expectBraille('11111 line h1', {startIndex: 7, endIndex: 10});

      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxEditingTest', 'EditableLineOneStaticText', async function() {
      const root = await this.runWithLoadedTree(`
    <p contenteditable style="word-spacing:100000px">this is a test</p>
  `);
      const staticText = root.find({role: RoleType.STATIC_TEXT});

      let e = new EditableLine(staticText, 0, staticText, 0);
      assertEquals('this ', e.text);

      assertEquals(0, e.startOffset);
      assertEquals(0, e.endOffset);
      assertEquals(0, e.localStartOffset);
      assertEquals(0, e.localEndOffset);

      assertEquals(0, e.containerStartOffset);
      assertEquals(4, e.containerEndOffset);

      e = new EditableLine(staticText, 1, staticText, 1);
      assertEquals('this ', e.text);

      assertEquals(1, e.startOffset);
      assertEquals(1, e.endOffset);
      assertEquals(1, e.localStartOffset);
      assertEquals(1, e.localEndOffset);

      assertEquals(0, e.containerStartOffset);
      assertEquals(4, e.containerEndOffset);

      e = new EditableLine(staticText, 5, staticText, 5);
      assertEquals('is ', e.text);

      assertEquals(0, e.startOffset);
      assertEquals(0, e.endOffset);
      assertEquals(5, e.localStartOffset);
      assertEquals(5, e.localEndOffset);

      assertEquals(0, e.containerStartOffset);
      assertEquals(2, e.containerEndOffset);

      e = new EditableLine(staticText, 7, staticText, 7);
      assertEquals('is ', e.text);

      assertEquals(2, e.startOffset);
      assertEquals(2, e.endOffset);
      assertEquals(7, e.localStartOffset);
      assertEquals(7, e.localEndOffset);

      assertEquals(0, e.containerStartOffset);
      assertEquals(2, e.containerEndOffset);
    });

AX_TEST_F(
    'ChromeVoxEditingTest', 'EditableLineTwoStaticTexts', async function() {
      const root = await this.runWithLoadedTree(`
    <p contenteditable>hello <b>world</b></p>
  `);
      const text = root.find({role: RoleType.STATIC_TEXT});
      const bold = text.nextSibling;

      let e = new EditableLine(text, 0, text, 0);
      assertEquals('hello world', e.text);

      assertEquals(0, e.startOffset);
      assertEquals(0, e.endOffset);
      assertEquals(0, e.localStartOffset);
      assertEquals(0, e.localEndOffset);

      assertEquals(0, e.containerStartOffset);
      assertEquals(5, e.containerEndOffset);

      e = new EditableLine(text, 5, text, 5);
      assertEquals('hello world', e.text);

      assertEquals(5, e.startOffset);
      assertEquals(5, e.endOffset);
      assertEquals(5, e.localStartOffset);
      assertEquals(5, e.localEndOffset);

      assertEquals(0, e.containerStartOffset);
      assertEquals(5, e.containerEndOffset);

      e = new EditableLine(bold, 0, bold, 0);
      assertEquals('hello world', e.text);

      assertEquals(6, e.startOffset);
      assertEquals(6, e.endOffset);
      assertEquals(0, e.localStartOffset);
      assertEquals(0, e.localEndOffset);

      assertEquals(6, e.containerStartOffset);
      assertEquals(10, e.containerEndOffset);

      e = new EditableLine(bold, 4, bold, 4);
      assertEquals('hello world', e.text);

      assertEquals(10, e.startOffset);
      assertEquals(10, e.endOffset);
      assertEquals(4, e.localStartOffset);
      assertEquals(4, e.localEndOffset);

      assertEquals(6, e.containerStartOffset);
      assertEquals(10, e.containerEndOffset);
    });

AX_TEST_F('ChromeVoxEditingTest', 'EditableLineEquality', async function() {
  const root = await this.runWithLoadedTree(`
    <div contenteditable role="textbox">
      <p style="word-spacing:100000px">this is a test</p>
      <p>hello <b>world</b></p>
    </div>
  `);
  const thisIsATest = root.findAll({role: RoleType.PARAGRAPH})[0].firstChild;
  const hello = root.findAll({role: RoleType.PARAGRAPH})[1].firstChild;
  const world = root.findAll({role: RoleType.PARAGRAPH})[1].lastChild;

  // The same position -- sanity check.
  let e1 = new EditableLine(thisIsATest, 0, thisIsATest, 0);
  assertEquals('this ', e1.text);
  assertTrue(e1.isSameLine(e1));

  // Offset into the same soft line.
  let e2 = new EditableLine(thisIsATest, 1, thisIsATest, 1);
  assertTrue(e1.isSameLine(e2));

  // Boundary.
  e2 = new EditableLine(thisIsATest, 4, thisIsATest, 4);
  assertTrue(e1.isSameLine(e2));

  // Offsets into different soft lines.
  e2 = new EditableLine(thisIsATest, 5, thisIsATest, 5);
  assertEquals('is ', e2.text);
  assertFalse(e1.isSameLine(e2));

  // Different offsets into second soft line.
  e1 = new EditableLine(thisIsATest, 6, thisIsATest, 6);
  assertTrue(e1.isSameLine(e2));

  // Boundary.
  e1 = new EditableLine(thisIsATest, 7, thisIsATest, 7);
  assertTrue(e1.isSameLine(e2));

  // Third line.
  e1 = new EditableLine(thisIsATest, 8, thisIsATest, 8);
  assertEquals('a ', e1.text);
  assertFalse(e1.isSameLine(e2));

  // Last line.
  e2 = new EditableLine(thisIsATest, 10, thisIsATest, 10);
  assertEquals('test', e2.text);
  assertFalse(e1.isSameLine(e2));

  // Boundary.
  e1 = new EditableLine(thisIsATest, 13, thisIsATest, 13);
  assertTrue(e1.isSameLine(e2));

  // Cross into new paragraph.
  e2 = new EditableLine(hello, 0, hello, 0);
  assertEquals('hello world', e2.text);
  assertFalse(e1.isSameLine(e2));

  // On same node, with multi-static text line.
  e1 = new EditableLine(hello, 1, hello, 1);
  assertTrue(e1.isSameLine(e2));

  // On same node, with multi-static text line; boundary.
  e1 = new EditableLine(hello, 5, hello, 5);
  assertTrue(e1.isSameLine(e2));

  // On different node, with multi-static text line.
  e1 = new EditableLine(world, 1, world, 1);
  assertTrue(e1.isSameLine(e2));

  // Another mix of lines.
  e2 = new EditableLine(thisIsATest, 9, thisIsATest, 9);
  assertFalse(e1.isSameLine(e2));
});

AX_TEST_F(
    'ChromeVoxEditingTest', 'EditableLineStrictEquality', async function() {
      const root = await this.runWithLoadedTree(`
    <div contenteditable role="textbox">
      <p style="word-spacing:100000px">this is a test</p>
      <p>hello <b>world</b></p>
    </div>
  `);
      const thisIsATest =
          root.findAll({role: RoleType.PARAGRAPH})[0].firstChild;
      const hello = root.findAll({role: RoleType.PARAGRAPH})[1].firstChild;
      const world = root.findAll({role: RoleType.PARAGRAPH})[1].lastChild;

      // The same position -- sanity check.
      let e1 = new EditableLine(thisIsATest, 0, thisIsATest, 0);
      assertEquals('this ', e1.text);
      assertTrue(e1.isSameLineAndSelection(e1));

      // Offset into the same soft line.
      let e2 = new EditableLine(thisIsATest, 1, thisIsATest, 1);
      assertFalse(e1.isSameLineAndSelection(e2));

      // Boundary.
      e2 = new EditableLine(thisIsATest, 4, thisIsATest, 4);
      assertFalse(e1.isSameLineAndSelection(e2));

      // Offsets into different soft lines.
      e2 = new EditableLine(thisIsATest, 5, thisIsATest, 5);
      assertEquals('is ', e2.text);
      assertFalse(e1.isSameLineAndSelection(e2));

      // Sanity check; second soft line.
      assertTrue(e2.isSameLineAndSelection(e2));

      // Different offsets into second soft line.
      e1 = new EditableLine(thisIsATest, 6, thisIsATest, 6);
      assertFalse(e1.isSameLineAndSelection(e2));

      // Boundary.
      e1 = new EditableLine(thisIsATest, 7, thisIsATest, 7);
      assertFalse(e1.isSameLineAndSelection(e2));

      // Cross into new paragraph.
      e2 = new EditableLine(hello, 0, hello, 0);
      assertEquals('hello world', e2.text);
      assertFalse(e1.isSameLineAndSelection(e2));

      // On same node, with multi-static text line.
      e1 = new EditableLine(hello, 1, hello, 1);
      assertFalse(e1.isSameLineAndSelection(e2));

      // On same node, with multi-static text line; boundary.
      e1 = new EditableLine(hello, 5, hello, 5);
      assertFalse(e1.isSameLineAndSelection(e2));
    });

AX_TEST_F(
    'ChromeVoxEditingTest', 'EditableLineBaseLineAnchorOrFocus',
    async function() {
      const root = await this.runWithLoadedTree(`
    <div contenteditable role="textbox">
      <p style="word-spacing:100000px">this is a test</p>
      <p>hello <b>world</b></p>
    </div>
  `);
      const thisIsATest =
          root.findAll({role: RoleType.PARAGRAPH})[0].firstChild;
      const hello = root.findAll({role: RoleType.PARAGRAPH})[1].firstChild;
      const world = root.findAll({role: RoleType.PARAGRAPH})[1].lastChild;

      // The same position -- sanity check.
      let e1 = new EditableLine(thisIsATest, 0, thisIsATest, 0, true);
      assertEquals('this ', e1.text);

      // Offsets into different soft lines; base on focus (default).
      e1 = new EditableLine(thisIsATest, 0, thisIsATest, 6);
      assertEquals('is ', e1.text);
      // Notice that the offset is truncated at the beginning of the line.
      assertEquals(0, e1.startOffset);
      // Notice that the end offset is properly retained.
      assertEquals(1, e1.endOffset);

      // Offsets into different soft lines; base on anchor.
      e1 = new EditableLine(thisIsATest, 0, thisIsATest, 6, true);
      assertEquals('this ', e1.text);
      assertEquals(0, e1.startOffset);
      // Notice            that the end offset is truncated up to the end of
      // line.
      assertEquals(5, e1.endOffset);

      // Across paragraph selection with base line on focus.
      e1 = new EditableLine(thisIsATest, 5, hello, 2);
      assertEquals('hello world', e1.text);
      assertEquals(0, e1.startOffset);
      assertEquals(2, e1.endOffset);

      // Across paragraph selection with base line on anchor.
      e1 = new EditableLine(thisIsATest, 5, hello, 2, true);
      assertEquals('is ', e1.text);
      assertEquals(0, e1.startOffset);
      assertEquals(3, e1.endOffset);
    });

AX_TEST_F('ChromeVoxEditingTest', 'IsValidLine', async function() {
  const root = await this.runWithLoadedTree(`
    <div contenteditable role="textbox">
      <p style="word-spacing:100000px">this is a test</p>
      <p>end</p>
    </div>
  `);
  // Each word is on its own line, but parented by a static text.
  const [text, endText] = root.findAll({role: RoleType.STATIC_TEXT});

  // The EditableLine object automatically adjusts to surround the line no
  // matter what the input is.
  const line = new EditableLine(text, 0, text, 0);
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

AX_TEST_F('ChromeVoxEditingTest', 'TelTrimsWhitespace', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(`
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
  `);
  await this.focusFirstTextField(root);

  const go = root.find({role: RoleType.GENERIC_CONTAINER});
  const enterKey = go.doDefault.bind(go);

  mockFeedback.call(enterKey)
      .expectSpeech('6')
      .call(enterKey)
      .expectSpeech('0')
      .call(enterKey)
      .expectSpeech('1')

      // Deletion.
      .call(enterKey)
      .expectSpeech('1');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxEditingTest', 'BackwardWordDelete', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(`
    <div
        style='max-width: 5px; overflow-wrap: normal'
        contenteditable>
      this is a test
    </div>
  `);
  await this.focusFirstTextField(
      root, {attributes: {nonAtomicTextFieldRoot: true}});

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
      .expectBraille(' mled', {startIndex: 0, endIndex: 0});
  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxEditingTest', 'BackwardWordDeleteAcrossParagraphs',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(`
    <div
        style='max-width: 5px; overflow-wrap: normal'
        contenteditable
        role="textbox">
      <p>first line</p>
      <p>second line</p>
    </div>
  `);
      await this.focusFirstTextField(root);

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
          .expectSpeech('first , deleted');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxEditingTest', 'GrammarErrors', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(`
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
  `);
  await this.focusFirstTextField(root);

  const go = root.find({role: RoleType.BUTTON});
  const moveByChar = go.doDefault.bind(go);

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
      .expectSpeech(' ', 'Leaving grammar error');

  await mockFeedback.replay();
});

// Flaky test, crbug.com/1098642.
AX_TEST_F(
    'ChromeVoxEditingTest', 'DISABLED_CharacterTypedAfterNewLine',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(`
    <p>start</p>
    <div contenteditable role="textbox">
      <p>hello</p>
    </div>
  `);
      await this.focusFirstTextField(root);

      mockFeedback.call(this.press(KeyCode.END, {ctrl: true}))
          .expectSpeech('hello')
          .call(this.press(KeyCode.RETURN))
          .expectSpeech('\n')
          .call(this.press(KeyCode.A))
          .expectSpeech('a');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxEditingTest', 'SelectAll', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(`
    <div contenteditable role="textbox">
      <p>first line</p>
      <p>second line</p>
      <p>third line</p>
    </div>
  `);
  await this.focusFirstTextField(root);

  mockFeedback.call(this.press(KeyCode.END, {ctrl: true}))
      .expectSpeech('third line')
      .call(this.press(KeyCode.A, {ctrl: true}))
      .expectSpeech('first line', 'second line', 'third line', 'selected')
      .call(this.press(KeyCode.UP))
      .expectSpeech('second line')
      .call(this.press(KeyCode.A, {ctrl: true}))
      .expectSpeech('first line', 'second line', 'third line', 'selected')
      .call(this.press(KeyCode.HOME, {ctrl: true}))
      .expectSpeech('first line')
      .call(this.press(KeyCode.A, {ctrl: true}))
      .expectSpeech('first line', 'second line', 'third line', 'selected');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxEditingTest', 'TextAreaBrailleEmptyLine', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree('<textarea></textarea>');
  const textarea = await this.focusFirstTextField(root);
  textarea.setValue('test\n\none\ntwo\n\nthree');
  await this.waitForEvent(textarea, 'valueInTextFieldChanged');
  mockFeedback.call(this.press(KeyCode.UP)).expectBraille('\n');
  mockFeedback.call(this.press(KeyCode.UP)).expectBraille('two\n');
  mockFeedback.call(this.press(KeyCode.UP)).expectBraille('one\n');
  mockFeedback.call(this.press(KeyCode.UP)).expectBraille('\n');
  mockFeedback.call(this.press(KeyCode.UP)).expectBraille('test\nmled');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxEditingTest', 'MoveByCharacterIntent', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(`
    <div contenteditable role="textbox">
      <p>123</p>
      <p>456</p>
    </div>
  `);
  await this.focusFirstTextField(root);

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
      .expectSpeech('3');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxEditingTest', 'MoveByLineIntent', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(`
    <div contenteditable role="textbox">
      <p>123</p>
      <p>456</p>
      <p>789</p>
    </div>
  `);
  await this.focusFirstTextField(root);

  mockFeedback.call(this.press(KeyCode.DOWN))
      .expectSpeech('456')
      .call(this.press(KeyCode.DOWN))
      .expectSpeech('789')
      .call(this.press(KeyCode.UP))
      .expectSpeech('456')
      .call(this.press(KeyCode.UP))
      .expectSpeech('123');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxEditingTest', 'SelectAllBareTextContent', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(`
    <div contenteditable role="textbox">unread</div>
  `);
  await this.focusFirstTextField(root);

  mockFeedback.call(this.press(KeyCode.END, {ctrl: true}))
      .expectSpeech('unread')
      .call(this.press(KeyCode.A, {ctrl: true}))
      .expectSpeech('unread', 'selected');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxEditingTest', 'InputEvents', async function() {
  const site = `<input type="text"></input>`;
  const root = await this.runWithLoadedTree(site);
  const input = await this.focusFirstTextField(root);

  // EventType.TEXT_SELECTION_CHANGED fires on focus as well.
  event = await this.waitForEditableEvent();
  assertEquals(EventType.TEXT_SELECTION_CHANGED, event.type);
  assertEquals(input, event.target);
  assertEquals('', input.value);

  this.press(KeyCode.A)();

  // We deliberately use EventType.TEXT_SELECTION_CHANGED instead of
  // EventType.DOCUMENT_SELECTION_CHANGED for text fields.
  event = await this.waitForEditableEvent();
  assertEquals(EventType.TEXT_SELECTION_CHANGED, event.type);
  assertEquals(input, event.target);
  assertEquals('a', input.value);

  event = await this.waitForEditableEvent();
  assertEquals(EventType.VALUE_IN_TEXT_FIELD_CHANGED, event.type);
  assertEquals(input, event.target);
  assertEquals('a', input.value);

  this.press(KeyCode.B)();

  event = await this.waitForEditableEvent();
  assertEquals(EventType.TEXT_SELECTION_CHANGED, event.type);
  assertEquals(input, event.target);
  assertEquals('ab', input.value);

  event = await this.waitForEditableEvent();
  assertEquals(EventType.VALUE_IN_TEXT_FIELD_CHANGED, event.type);
  assertEquals(input, event.target);
  assertEquals('ab', input.value);
});

AX_TEST_F('ChromeVoxEditingTest', 'TextAreaEvents', async function() {
  const site = `<textarea></textarea>`;
  const root = await this.runWithLoadedTree(site);
  const textArea = await this.focusFirstTextField(root);
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

AX_TEST_F('ChromeVoxEditingTest', 'ContentEditableEvents', async function() {
  const site = `<div role="textbox" contenteditable></div>`;
  const root = await this.runWithLoadedTree(site);
  const contentEditable = await this.focusFirstTextField(root);
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

AX_TEST_F('ChromeVoxEditingTest', 'MarkedContent', async function() {
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
  const root = await this.runWithLoadedTree(site);
  await this.focusFirstTextField(root);

  mockFeedback.call(this.press(KeyCode.DOWN))
      .expectSpeech('This is ')
      .expectSpeech('Marked content', 'my', 'Marked content end')
      .call(this.press(KeyCode.DOWN))
      .expectSpeech('This is ')
      .expectSpeech('Comment', 'your', 'Comment end')
      .call(this.press(KeyCode.DOWN))
      .expectSpeech('This is ')
      .expectSpeech('Suggest', 'Insert', 'their', 'Insert end', 'Suggest end')
      .call(this.press(KeyCode.DOWN))
      .expectSpeech('This is ')
      .expectSpeech(
          'Suggest', 'Delete', `everyone's`, 'Delete end', 'Suggest end');
  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxEditingTest', 'NestedInsertionDeletion', async function() {
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
  const root = await this.runWithLoadedTree(site);
  await this.focusFirstTextField(root);

  mockFeedback.call(this.press(KeyCode.DOWN))
      .expectSpeech(
          'I ', 'Suggest', 'Username', 'Insert', 'was', 'Insert end', 'Delete',
          'am', 'Delete end', 'Suggest end', ' typing')
      .call(this.press(KeyCode.DOWN))
      .expectSpeech('End');
  await mockFeedback.replay();
});

// TODO(b/321663219): Re-enable when flakiness is resolved.
AX_TEST_F(
    'ChromeVoxEditingTest', 'DISABLED_MoveByCharSuggestions', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <div contenteditable="true" role="textbox">
      <p>Start</p>
      <span>I </span>
      <span role="suggestion" aria-description="Username">
        <span role="insertion">was</span>
        <span role="deletion">am</span></span><span> typing</span>
      <p>End</p>
    </div>
  `;
      const root = await this.runWithLoadedTree(site);
      await this.focusFirstTextField(root);

      mockFeedback.call(this.press(KeyCode.DOWN))
          .expectSpeech('I ')
          // Move forward through line.
          .call(this.press(KeyCode.RIGHT))
          .expectSpeech(' ')
          .call(this.press(KeyCode.RIGHT))
          .expectSpeech('Suggest', 'Username', 'Insert', 'w')
          .call(this.press(KeyCode.RIGHT))
          .expectSpeech('a')
          .call(this.press(KeyCode.RIGHT))
          .expectSpeech('s')
          .expectSpeech('Insert end')
          .call(this.press(KeyCode.RIGHT))
          .call(this.press(KeyCode.RIGHT))
          .expectSpeech('Delete', 'a')
          .call(this.press(KeyCode.RIGHT))
          .expectSpeech('m')
          .expectSpeech('Delete end', 'Suggest end')
          // Move backward through the same line.
          .call(this.press(KeyCode.LEFT))
          .expectSpeech('Delete', 'a')
          .call(this.press(KeyCode.LEFT))
          .call(this.press(KeyCode.LEFT))
          .expectSpeech('s', 'Insert end')
          .call(this.press(KeyCode.LEFT))
          .expectSpeech('a')
          .call(this.press(KeyCode.LEFT))
          .expectSpeech('Suggest', 'Insert', 'w')
          .call(this.press(KeyCode.DOWN))
          .expectSpeech('End');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxEditingTest', 'MoveByWordSuggestions', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <div contenteditable="true" role="textbox">
      <p>Start</p>
      <span>I </span>
      <span role="suggestion" aria-description="Username">
        <span role="insertion">was</span>
        <span role="deletion">am</span></span><span> typing</span>
      <p>End</p>
    </div>
  `;
      const root = await this.runWithLoadedTree(site);
      await this.focusFirstTextField(root);

      mockFeedback.call(this.press(KeyCode.DOWN))
          .expectSpeech('I ')
          // Move forward through line.
          .call(this.press(KeyCode.RIGHT, {ctrl: true}))
          .expectSpeech('I')
          .call(this.press(KeyCode.RIGHT, {ctrl: true}))
          .expectSpeech('Suggest', 'Username', 'Insert', 'was', 'Insert end')
          .call(this.press(KeyCode.RIGHT, {ctrl: true}))
          .expectSpeech('Delete', 'am', 'Delete end', 'Suggest end')
          // Move backward through line.
          .call(this.press(KeyCode.LEFT, {ctrl: true}))
          .expectSpeech('Delete', 'am', 'Delete end', 'Suggest end')
          .call(this.press(KeyCode.LEFT, {ctrl: true}))
          .expectSpeech('Suggest', 'Username', 'Insert', 'was')
          .call(this.press(KeyCode.LEFT, {ctrl: true}))
          .expectSpeech('I')
          .call(this.press(KeyCode.DOWN))
          .expectSpeech('End');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxEditingTest', 'MoveByWordSuggestionsNoIntents', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <div contenteditable="true" role="textbox" id="textbox">
      <p>Start</p>
      <span>I </span>
      <span role="suggestion" aria-description="Username">
        <span role="insertion">was</span>
        <span role="deletion">am</span></span><span> typing</span>
      <p>End</p>
    </div>
    <script>
      const textbox = document.getElementById('textbox');
      let firstRightSkipped = false;
      textbox.addEventListener('keydown', event => {
        if (event.keyCode === 40) {
          return;
        }

        if (!firstRightSkipped) {
          firstRightSkipped = true;
          return;
        }

        event.preventDefault();
        event.stopPropagation();

        const contentEditable = document.activeElement;
        const selection = getSelection();
        selection.removeAllRanges();
        const range = new Range();
        const text = contentEditable.children[2].children[0].firstChild;
        range.setStart(text, 3);
        range.setEnd(text, 3);
        selection.addRange(range);
      });
    </script>
  `;
      const root = await this.runWithLoadedTree(site);
      await this.focusFirstTextField(root);

      mockFeedback.call(this.press(KeyCode.DOWN))
          .expectSpeech('I ')
          // Move forward through line.

          // This first right arrow is allowed to be processed by the content
          // editable.
          .call(this.press(KeyCode.RIGHT, {ctrl: true}))
          .expectSpeech('I')

          // This next right is swallowed by the content editable mimicking
          // custom rich editors. It manually moves selection (and looses intent
          // data). We infer it by getting a command mapped for a raw control
          // right arrow key.
          .call(doCmd('nativeNextWord'))
          .call(this.press(KeyCode.RIGHT, {ctrl: true}))
          .expectSpeech('Suggest', 'Username', 'Insert', 'was', 'Insert end');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxEditingTest', 'Separator', async function() {
  // In the past, an ARIA leaf role would cause subtree content to be removed.
  // However, the new decision is to not remove any content the user might
  // interact with.
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div contenteditable="true" role="textbox">
      <p>Start</p>
      <p><span>Hello</span></p>
      <p><span role="separator">-</span></p>
      <p><span>World</span></p>
    </div>
  `;
  const root = await this.runWithLoadedTree(site);
  await this.focusFirstTextField(root);

  mockFeedback.call(this.press(KeyCode.DOWN))
      .expectSpeech('Hello')
      .call(this.press(KeyCode.DOWN))
      .expectSpeech('-', 'Separator')
      .call(this.press(KeyCode.DOWN))
      .expectSpeech('World')

      .call(this.press(KeyCode.LEFT))
      .expectNextSpeechUtteranceIsNot('\n')
      // This reads the entire line (just one character).
      .expectSpeech('-', 'Separator')

      .call(this.press(KeyCode.LEFT))
      // This reads the single character.
      .expectSpeech('-')

      .call(this.press(KeyCode.LEFT))
      // Notice this reads the entire line which is generally undesirable
      // except for special cases like this.
      .expectSpeech('Hello');

  await mockFeedback.replay();
});

// Test for the issue in crbug.com/1203840. This case was causing an infinite
// loop in ChromeVox's editable line data computation. This test ensures we
// workaround potential infinite loops correctly, and should be removed once the
// proper fix is implemented in blink.
AX_TEST_F(
    'ChromeVoxEditingTest', 'EditableLineInfiniteLoopWorkaround',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <div contenteditable="true" role="textbox">
      <p>Start</p>
      <table>
        <tbody>
          <tr>
            <td>
              <span>
                <span style="font-size:13.333333333333332px;">This is a test<span>&nbsp;</span></span></span>
            </td>
          </tr>
        </tbody>
      </table>
      <span>End</span>
    </div>
  `;
      const root = await this.runWithLoadedTree(site);
      await this.focusFirstTextField(root);

      mockFeedback.call(this.press(KeyCode.DOWN))
          .expectSpeech('This is a test')
          .call(this.press(KeyCode.DOWN))
          .expectSpeech('End');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxEditingTest', 'TextEditHandlerCreatesAutomationEditable',
    async function() {
      const site = `
    <input type="text"></input>
  `;
      const root = await this.runWithLoadedTree(site);
      const input = await this.focusFirstTextField(root);

      // The initial real input is a simple non-rich text field.
      assertEquals(
          'AutomationEditableText',
          DesktopAutomationInterface.instance.textEditHandler.editableText_
              .constructor.name,
          'Real text field was not a non-rich text.');

      // Now, we will override some properties directly to
      // ensure we don't depend on Blink's behaviors which can change based
      // on style. We want to work directly with only the automation api
      // itself to ensure we have full coverage.
      let htmlTag = '';
      let state = {};
      let nonAtomicTextFieldRoot = false;
      Object.defineProperty(input, 'htmlTag', {get: () => htmlTag});
      Object.defineProperty(input, 'state', {get: () => state});
      Object.defineProperty(
          input, 'nonAtomicTextFieldRoot', {get: () => nonAtomicTextFieldRoot});

      // An invalid editable.
      let didThrow = false;
      let handler;
      try {
        handler = new TextEditHandler(input);
      } catch (e) {
        didThrow = true;
      }
      assertTrue(didThrow, 'Non-editable created editable handler.');

      // A simple editable.
      htmlTag = '';
      state = {editable: true};
      nonAtomicTextFieldRoot = false;
      handler = new TextEditHandler(input);
      assertEquals(
          'AutomationEditableText', handler.editableText_.constructor.name,
          'Incorrect backing object for simple editable.');

      // A non-rich editable via multiline.
      htmlTag = '';
      state = {editable: true, multiline: true};
      nonAtomicTextFieldRoot = false;
      handler = new TextEditHandler(input);
      assertEquals(
          'AutomationEditableText', handler.editableText_.constructor.name,
          'Incorrect object for multiline editable.');

      // A rich editable via textarea tag.
      htmlTag = 'textarea';
      state = {editable: true};
      nonAtomicTextFieldRoot = false;
      handler = new TextEditHandler(input);
      assertEquals(
          'RichEditableText', handler.editableText_.constructor.name,
          'Incorrect object for textarea html tag.');

      // A rich editable via state.
      htmlTag = '';
      state = {editable: true, richlyEditable: true};
      nonAtomicTextFieldRoot = false;
      handler = new TextEditHandler(input);
      assertEquals(
          'RichEditableText', handler.editableText_.constructor.name,
          'Incorrect object for richly editable state.');

      // A rich editable via contenteditable. (aka <div contenteditable>).
      htmlTag = 'div';
      state = {editable: true};
      nonAtomicTextFieldRoot = true;
      handler = new TextEditHandler(input);
      assertEquals(
          'RichEditableText', handler.editableText_.constructor.name,
          'Incorrect object for content editable.');

      // Note that it is not possible to have <div
      // contenteditable="someInvalidValue"> or <div contenteditable=false>
      // and still have the div expose editable state, so we never check
      // that.
    });

// TODO(crbug.com/40794522): flakes due to underlying bug with
// accessibility intents.
AX_TEST_F(
    'ChromeVoxEditingTest', 'DISABLED_ParagraphNavigation', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <div contenteditable role="textbox"
        style='max-width: 5px; overflow-wrap: normal'>
      <p>This is paragraph number one.</p>
      <p>Another paragraph, number two.</p>
    </div>
  `;
      const root = await this.runWithLoadedTree(site);
      await this.focusFirstTextField(root);

      // We bind specific callbacks to send keys here because EventGenerator
      // (which sends key down and up) does not seem to work with these
      // shortcuts.
      const ctrlDown = () => chrome.accessibilityPrivate.sendSyntheticKeyEvent({
        type: chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN,
        keyCode: KeyCode.DOWN,
        modifiers: {ctrl: true},
      });
      const ctrlUp = () => chrome.accessibilityPrivate.sendSyntheticKeyEvent({
        type: chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN,
        keyCode: KeyCode.UP,
        modifiers: {ctrl: true},
      });

      mockFeedback.expectSpeech('Text area')
          .call(ctrlDown)
          .expectSpeech('Another paragraph, number two.')
          .call(this.press(KeyCode.DOWN))
          .expectSpeech('paragraph, ')
          .call(ctrlUp)
          .expectSpeech('This is paragraph number one.')
          .call(this.press(KeyCode.UP))
          .expectSpeech('number ')
          .call(this.press(KeyCode.UP))
          .expectSpeech('paragraph ')
          .call(ctrlDown)
          .expectSpeech('Another paragraph, number two.')
          .call(this.press(KeyCode.DOWN))
          .expectSpeech('paragraph, ');
      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxEditingTest', 'StartAndEndOfOutputStopAtEditableRoot',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <div role="article">
      <div contenteditable role="textbox">
        hello<br>world
      </div>
    </div>
  `;
      const root = await this.runWithLoadedTree(site);
      await this.focusFirstTextField(root);
      mockFeedback.expectSpeech('Text area')
          .call(this.press(KeyCode.DOWN))
          .expectSpeech('world')
          .call(this.press(KeyCode.UP))
          .expectNextSpeechUtteranceIsNot('Article')
          .expectNextSpeechUtteranceIsNot('Article end')
          .expectSpeech('hello');
      await mockFeedback.replay();
    });

// crbug.com/1356181 Disable due to flaky.
AX_TEST_F('ChromeVoxEditingTest', 'DISABLED_TableNavigation', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div contenteditable role="textbox" tabindex=0>
      <table border=1>
        <tr><td>hello<br>world</td><td>goodbye</td></tr>
        <tr><td>hola</td><td>hasta luego</td></tr>
      </table>
    </div>
  `;
  const root = await this.runWithLoadedTree(site);
  await this.focusFirstTextField(root);

  mockFeedback.expectSpeech('Text area')
      .call(this.press(KeyCode.RIGHT))
      .call(this.press(KeyCode.RIGHT))
      .expectSpeech('e')
      .call(doCmd('nextCol'))
      .expectSpeech('goodbye')
      .expectSpeech('row 1 column 2')
      .call(this.press(KeyCode.RIGHT))
      .expectSpeech('o')
      .call(doCmd('previousCol'))
      .expectSpeech('hello', 'world')
      .expectSpeech('row 1 column 1')
      .call(this.press(KeyCode.RIGHT))
      .expectSpeech('e');
  await mockFeedback.replay();
});

AX_TEST_F(
    'ChromeVoxEditingTest', 'InputTextBrailleContractions', async function() {
      const site = `
    <input type=text value="about that"></input>
  `;
      const root = await this.runWithLoadedTree(site);
      await this.focusFirstTextField(root);

      // In case LibLouis takes a while to load.
      if (!BrailleTranslatorManager.instance.liblouis_.isLoaded()) {
        await new Promise(
            resolve =>
                BrailleTranslatorManager.instance.liblouis_.onInstanceLoad_ =
                    resolve);
      }

      // Fake an available display.
      BrailleDisplayManager.instance.refreshDisplayState_(
          {available: true, textRowCount: 1, textColumnCount: 40});

      // Set braille to use 6-dot braille (which is defaulted to UEB grade 2
      // contracted braille).
      SettingsManager.set('brailleTable', 'en-ueb-g2');

      // Wait for it to be fully refreshed (liblouis loads the new tables, our
      // translators are re-created).
      await BrailleTranslatorManager.instance.loadTablesForTest();

      // Fake an available display.
      BrailleDisplayManager.instance.refreshDisplayState_(
          {available: true, textRowCount: 1, textColumnCount: 40});

      // Set braille to use 6-dot braille (which is defaulted to UEB grade 2
      // contracted braille).
      SettingsManager.set('brailleTable', 'en-ueb-g2');
      await new Promise(
          resolve => BrailleTranslatorManager.instance.refresh(
              SettingsManager.getString('brailleTable'), undefined, resolve));

      async function waitForBrailleDots(expectedDots) {
        return new Promise(r => {
          chrome.brailleDisplayPrivate.writeDots = dotsBuffer => {
            const view = new Uint8Array(dotsBuffer);
            const dots = new Array(view.length);
            view.forEach((item, index) => dots[index] = item.toString(2));
            if (expectedDots.toString() === dots.toString()) {
              r();
            }
          };
        });
      }

      this.press(KeyCode.END)();

      // This test intentionally leaves the raw binary encoding for braille.
      // Dots are read from right to left.
      await waitForBrailleDots([
        // 'ab' is 'about' in UEB Grade 2.
        1 /* a */, 11 /* b */,

        0 /* space */,

        11110 /* t */, 10011 /* h */, 1 /* a */, 11110 /* t */,

        11000000 /* cursor _ */,

        101011, /* ed contraction */
      ]);

      this.press(KeyCode.HOME)();
      await waitForBrailleDots([
        11000001 /* a with a cursor _*/, 11 /* b */, 10101 /* o */,
        100101 /* u */, 11110 /* t */,

        0 /* space */,

        // 't' by itself is contracted as 'that'.
        11110 /* t */,

        0 /* space */,

        101011, /* ed contraction */
      ]);
    });


AX_TEST_F('ChromeVoxEditingTest', 'ContextMenus', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <textarea>abc</textarea>
  `;
  const root = await this.runWithLoadedTree(site);
  await this.focusFirstTextField(root);

  const textField = root.find({role: RoleType.TEXT_FIELD});
  mockFeedback.expectSpeech('Text area')
      .call(() => {
        textField.setSelection(0, 2);
      })
      .expectSpeech('ab', 'selected')
      .call(doCmd('contextMenu'))
      .expectSpeech(' menu opened')
      .call(this.press(KeyCode.ESCAPE))
      .expectSpeech('ab', 'selected');
  await mockFeedback.replay();
});

// TODO(b/321663219): Re-enable when flakiness is resolved.
AX_TEST_F(
    'ChromeVoxEditingTest', 'DISABLED_NativeCharWordCommands',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <p>start</p>
    <div role="textbox" contenteditable>This is a test</div>
  `;
      const root = await this.runWithLoadedTree(site);
      await this.focusFirstTextField(root);

      const textField = root.find({role: RoleType.TEXT_FIELD});
      mockFeedback.expectSpeech('Text area')
          .call(this.press(KeyCode.HOME, {ctrl: true}))
          .call(this.press(KeyCode.RIGHT))
          .expectSpeech('h')
          .call(this.press(KeyCode.RIGHT))
          .expectSpeech('i')
          .call(this.press(KeyCode.LEFT))
          .expectSpeech('h')

          .call(this.press(KeyCode.RIGHT, {ctrl: true}))
          .expectSpeech(/This\s*/)
          .call(this.press(KeyCode.RIGHT, {ctrl: true}))
          .expectSpeech('is')
          .call(this.press(KeyCode.LEFT, {ctrl: true}))
          .expectSpeech('is')
          .call(this.press(KeyCode.LEFT, {ctrl: true}))
          .expectSpeech(/This\s*/);

      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxEditingTest', 'TablesWithEmptyCells', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <div contenteditable="true" role="textbox">
      <p>A</p>
      <div><table>
        <colgroup><col><col></colgroup>
        <tbody>
          <tr>
            <td><div><span>&nbsp;</span></div></td>
            <td><div><span>&nbsp;</span></div></td>
          </tr>
          <tr>
            <td><div><span>&nbsp;</span></div></td>
            <td><div><span>&nbsp;</span></div></td>
          </tr>
        </tbody>
      </table>
    </div><div></div></div>
  `;
  const root = await this.runWithLoadedTree(site);
  await this.focusFirstTextField(root);
  const textField = root.find({role: RoleType.TEXT_FIELD});
  const table = textField.lastChild;
  const [row1, row2] = table.children;
  const [cell11, cell12] = row1.children;
  const [cell21, cell22] = row2.children;

  mockFeedback.expectSpeech('Text area')
      .call(() => textField.setSelection(0, 1))
      .expectSpeech('A', 'selected')

      // Non-breaking spaces (\u00a0) get preprocessed later by PrimaryTts
      // to ' '. This comes as part of speak line output in RichEditableText.
      .call(doCmd('nativeNextCharacter'))
      .call(() => textField.setSelection(1, 1))
      .expectSpeech('\u00a0', 'row 1 column 1')

      .call(doCmd('nativeNextCharacter'))
      .call(() => cell12.setSelection(0, 0))
      .expectSpeech('\u00a0', 'row 1 column 2')

      .call(doCmd('nativeNextCharacter'))
      .call(() => cell21.setSelection(0, 0))
      .expectSpeech('\u00a0', 'row 2 column 1')

      .call(doCmd('nativeNextCharacter'))
      .call(() => cell22.setSelection(0, 0))
      .expectSpeech('\u00a0', 'row 2 column 2');

  await mockFeedback.replay();
});

// TODO(b/321663219): Re-enable when flakiness is resolved.
AX_TEST_F(
    'ChromeVoxEditingTest', 'DISABLED_NonbreakingSpaceNewLineOrSpace',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <div contenteditable="true" role="textbox">
      <p>first line</p>
      <div><span>&nbsp;</span></div>
      <div><span>&nbsp;</span></div>
      <div><span>&nbsp;</span></div>
      <p>last line</p>
    </div>
  `;
      const root = await this.runWithLoadedTree(site);
      await this.focusFirstTextField(root);

      const textField = root.find({role: RoleType.TEXT_FIELD});
      mockFeedback.expectSpeech('Text area')
          .call(this.press(KeyCode.HOME, {ctrl: true}))
          .call(this.press(KeyCode.DOWN))
          .expectSpeech('\n')
          .call(this.press(KeyCode.DOWN))
          .expectSpeech('\n')
          .call(this.press(KeyCode.DOWN))
          .expectSpeech('\n')
          .call(this.press(KeyCode.DOWN))
          .expectSpeech('last line')

          .call(this.press(KeyCode.UP))
          .expectSpeech('\n')
          .call(this.press(KeyCode.UP))
          .expectSpeech('\n')
          .call(this.press(KeyCode.UP))
          .expectSpeech('\n')
          .call(this.press(KeyCode.UP))
          .expectSpeech('first line')

          .call(this.press(KeyCode.DOWN))
          .expectSpeech('\n')

          .call(this.press(KeyCode.RIGHT))
          .expectSpeech('\u00a0')
          .call(this.press(KeyCode.RIGHT))
          .expectSpeech('\u00a0')
          .call(this.press(KeyCode.RIGHT))
          .expectSpeech('\u00a0')
          .call(this.press(KeyCode.RIGHT))
          .expectSpeech('\u00a0')
          .call(this.press(KeyCode.RIGHT))
          .expectSpeech('\u00a0')
          .call(this.press(KeyCode.RIGHT))
          .expectSpeech('l')

          .call(this.press(KeyCode.LEFT))
          .expectSpeech('\u00a0')
          .call(this.press(KeyCode.LEFT))
          .expectSpeech('\u00a0')
          .call(this.press(KeyCode.LEFT))
          .expectSpeech('\u00a0')
          .call(this.press(KeyCode.LEFT))
          .expectSpeech('\u00a0')
          .call(this.press(KeyCode.LEFT))
          .expectSpeech('\u00a0')
          .call(this.press(KeyCode.LEFT))
          .expectSpeech('\u00a0')
          .call(this.press(KeyCode.LEFT))
          .expectSpeech('\n')
          .call(this.press(KeyCode.LEFT))
          .expectSpeech('e');

      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxEditingTest', 'JumpCommandsSyncSelection', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <div contenteditable="true" role="textbox">
      <p>first</p>
      <h1>second</h1>
      <p>third <a href="#">fourth</a></p>
      <table border=1><r><td>fifth</td></tr></table>
    </div>
  `;
      const root = await this.runWithLoadedTree(site);
      await this.focusFirstTextField(root);

      const textField = root.find({role: RoleType.TEXT_FIELD});
      mockFeedback.expectSpeech('Text area')
          .call(doCmd('nextTable'))
          .expectSpeech('fifth', 'row 1 column 1', 'Table , 1 by 1')

          // Verifies selection is where we expect.
          .call(this.press(KeyCode.RIGHT, {shift: true, ctrl: true}))
          .expectSpeech('fifth', 'row 1 column 1', 'Table , 1 by 1', 'selected')

          .call(doCmd('previousHeading'))
          .expectSpeech('second', 'Heading 1')
          .call(this.press(KeyCode.RIGHT, {shift: true, ctrl: true}))
          .expectSpeech('second', 'Heading 1', 'selected')

          .call(doCmd('nextLink'))
          .expectSpeech('fourth', 'Internal link')
          .call(this.press(KeyCode.RIGHT, {shift: true, ctrl: true}))
          .expectSpeech('fourth', 'Link', 'selected');

      await mockFeedback.replay();
    });

AX_TEST_F(
    'ChromeVoxEditingTest', 'BackspaceToEmptyTextField', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <p>start</p>
    <div aria-label="test" role="textbox" contenteditable></div>
  `;
      const root = await this.runWithLoadedTree(site);
      await this.focusFirstTextField(root);

      const textField = root.find({role: RoleType.TEXT_FIELD});
      mockFeedback.expectSpeech('Text area')
          .expectBraille('test mled', {startIndex: -1, endIndex: -1})
          .call(this.press(KeyCode.A))
          .expectBraille('a mled', {startIndex: 1, endIndex: 1})
          .call(this.press(KeyCode.BACK))
          .expectBraille(' mled', {startIndex: 0, endIndex: 0});

      await mockFeedback.replay();
    });

// Regression test that large text areas produce output.
// TODO(crbug.com/40944160): re-enable this test once its flakiness is resolved.
AX_TEST_F('ChromeVoxEditingTest', 'GiantTextAreaPerformance', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <textarea></textarea>
    <script>
      const codepointCount = 35536 * 10;
      const greeking1024Codepoints = 'Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum. Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse    cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum. Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua elit.';

      let value = '';
      while (value.length < codepointCount) {
        value += greeking1024Codepoints;
      }
      let textarea = document.querySelector('textarea');
      textarea.value = value;
      textarea.setSelectionRange(0, 0);
    </script>
  `;
  const root = await this.runWithLoadedTree(site);
  await this.focusFirstTextField(root);

  const textField = root.find({role: RoleType.TEXT_FIELD});
  mockFeedback.expectSpeech('Text area')
      .call(this.press(KeyCode.DOWN))
      .expectSpeech('amet, consectetur')
      .call(this.press(KeyCode.RIGHT))
      .expectSpeech('m')

      .replay();
});

AX_TEST_F(
    'ChromeVoxEditingTest', 'BrailleMoveByCharacterWord', async function() {
      const mockFeedback = this.createMockFeedback();
      const site = `
    <p>start</p>
    <div role="textbox" contenteditable><p>this is a test</p></div>
  `;
      const root = await this.runWithLoadedTree(site);
      await this.focusFirstTextField(root);

      const doBrailleEditCommand = command => () =>
          BrailleCommandHandler.onEditCommand_(command);
      const route = position => () => this.routeBraille(mockFeedback, position);

      const textField = root.find({role: RoleType.TEXT_FIELD});
      mockFeedback.expectSpeech('Text area')
          .expectBraille('this is a test mled', {startIndex: -1, endIndex: -1})

          .call(doBrailleEditCommand('nextWord'))
          .expectSpeech('this')
          .expectBraille('this is a test mled', {startIndex: 4, endIndex: 4})

          .call(doBrailleEditCommand('nextWord'))
          .expectSpeech('is')
          .expectBraille('this is a test mled', {startIndex: 7, endIndex: 7})

          .call(doBrailleEditCommand('nextWord'))
          .expectSpeech('a')
          .expectBraille('this is a test mled', {startIndex: 9, endIndex: 9})

          .call(doBrailleEditCommand('previousWord'))
          .expectSpeech('a')
          .expectBraille('this is a test mled', {startIndex: 8, endIndex: 8})

          .call(doBrailleEditCommand('previousWord'))
          .expectSpeech('is')
          .expectBraille('this is a test mled', {startIndex: 5, endIndex: 5})

          .call(doBrailleEditCommand('previousCharacter'))
          .expectSpeech(' ')
          .expectBraille('this is a test mled', {startIndex: 4, endIndex: 4})


          .call(doBrailleEditCommand('previousCharacter'))
          .expectSpeech('s')
          .expectBraille('this is a test mled', {startIndex: 3, endIndex: 3})

          .call(doBrailleEditCommand('previousCharacter'))
          .expectSpeech('i')
          .expectBraille('this is a test mled', {startIndex: 2, endIndex: 2})

          .call(doBrailleEditCommand('nextCharacter'))
          .expectSpeech('s')
          .expectBraille('this is a test mled', {startIndex: 3, endIndex: 3})

          .call(route(0))
          .expectSpeech('t')
          .expectBraille('this is a test mled', {startIndex: 0, endIndex: 0})

          .call(route(6))
          .expectSpeech('s')
          .expectBraille('this is a test mled', {startIndex: 6, endIndex: 6})

          .call(route(7))
          .expectSpeech(' ')
          .expectBraille('this is a test mled', {startIndex: 7, endIndex: 7});

      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxEditingTest', 'SelectAcrossSoftLineWraps', async function() {
  const mockFeedback = this.createMockFeedback();
  const site = `
    <p>start</p>
    <div role=textbox contenteditable>Copy this message into any text field. Move to the top, then select with shift+down arrow. Notice that text selection reads from the beginning of the very long line when pressing shift+down arrow. This continues to happen until encountering a line break
like this one.
    </div>
  `;
  const root = await this.runWithLoadedTree(site);
  await this.focusFirstTextField(root);

  const textField = root.find({role: RoleType.TEXT_FIELD});
  mockFeedback.expectSpeech('Text area')
      .call(this.press(KeyCode.DOWN, {shift: true}))
      .expectSpeech(
          'Copy this message into any text field. Move to the top, then select with shift+down arrow. Notice that text selection reads from the beginning of the very long line when ',
          'selected')
      .call(this.press(KeyCode.DOWN, {shift: true}))
      .expectSpeech(
          'pressing shift+down arrow. This continues to happen until encountering a line breaklike this one.',
          'selected');

  await mockFeedback.replay();
});

AX_TEST_F('ChromeVoxEditingTest', 'OnEvent', async function() {
  const setIntent = {command: IntentCommandType.SET_SELECTION};
  const clearIntent = {command: IntentCommandType.CLEAR_SELECTION};
  const otherIntent = {command: 'something else'};

  const root = await this.runWithLoadedTree('<input type=text>');
  await this.focusFirstTextField(root);
  const textField = root.find({role: RoleType.TEXT_FIELD});

  const handler = TextEditHandler.createForNode(textField);
  let receivedIntents;
  const captureIntents = intents => receivedIntents = intents;

  // If the event target is not focused, onEvent should exit early.
  handler.editableText_.onUpdate = captureIntents;
  handler.onEvent({target: {state: {}}});
  assertUndefined(receivedIntents);

  // If the event target is not the node given to the event handler, onEvent
  // should exit early.
  handler.editableText_.onUpdate = captureIntents;
  handler.onEvent({target: root});
  assertUndefined(receivedIntents);

  // Check that the intents are set, as expected, and onUpdate is called.
  textField.state.focused = true;
  handler.inferredIntents_ = ['b'];
  handler.editableText_.onUpdate = captureIntents;
  handler.onEvent({target: textField, intents: [otherIntent]});
  assertEquals(1, receivedIntents.length);
  assertEquals(otherIntent, receivedIntents[0]);

  // Check that inferred intents are used if no intents are provided.
  handler.inferredIntents_ = ['b'];
  receivedIntents = false;
  const intents = [];
  handler.onEvent({target: textField, intents});
  assertEquals(1, receivedIntents.length);
  assertEquals('b', receivedIntents[0]);

  // Check that inferred intents override provided intents if event.intents
  // contains SET_SELECTION.
  handler.inferredIntents_ = ['b'];
  receivedIntents = false;
  handler.onEvent({target: textField, intents: [setIntent, otherIntent]});
  assertEquals(1, receivedIntents.length);
  assertEquals('b', receivedIntents[0]);

  // Check that inferred intents override provided intents if event.intents
  // contains CLEAR_SELECTION.
  handler.inferredIntents_ = ['b'];
  receivedIntents = false;
  handler.onEvent({target: textField, intents: [otherIntent, clearIntent]});
  assertEquals(1, receivedIntents.length);
  assertEquals('b', receivedIntents[0]);
});
