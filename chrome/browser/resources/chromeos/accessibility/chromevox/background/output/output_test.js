// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);

/**
 * Gets the braille output and asserts that it matches expected values.
 * Annotations in the output that are primitive strings are ignored.
 */
function checkBrailleOutput(expectedText, expectedSpans, output) {
  const actualOutput = output.braille;
  // Remove string annotations.  These are tested in the speech output and
  // there's no need to clutter the tests with the corresponding braille
  // annotations.
  const actualSpans =
      actualOutput.spans_.filter(span => (typeof span.value !== 'string'));
  checkOutput_(
      expectedText, expectedSpans, actualOutput.toString(), actualSpans);
}

function checkSpeechOutput(expectedText, expectedSpans, output) {
  const actualOutput = output.speechOutputForTest;
  checkOutput_(
      expectedText, expectedSpans, actualOutput.toString(),
      actualOutput.spans_);
}

/** @private */
function checkOutput_(expectedText, expectedSpans, actualText, actualSpans) {
  assertEquals(expectedText, actualText);

  function describeSpanPrettyPrint(span) {
    return describeSpan(span)
        .replace(':', ': ')
        .replace('"value":', 'value:')
        .replace('"start":', 'start:')
        .replace('"end":', 'end:')
        .replace('"', '\'');
  }

  function describeSpan(span) {
    const obj = {value: span.value, start: span.start, end: span.end};
    if (obj.value instanceof OutputNodeSpan) {
      obj.value.node =
          (obj.value.node.name || '') + ' ' + obj.value.node.toString();
    }
    return JSON.stringify(obj);
  }

  function describeActualSpans() {
    return '\nAll actual spans:\n' +
        actualSpans.map(describeSpanPrettyPrint).join('\n');
  }

  function describeExpectedSpans() {
    return '\nAll expected spans:\n' +
        expectedSpans.map(describeSpanPrettyPrint).join('\n');
  }

  for (let i = 0, max = Math.max(expectedSpans.length, actualSpans.length);
       i < max; ++i) {
    const expectedSpan = expectedSpans[i];
    const actualSpan = actualSpans[i];
    if (!expectedSpan) {
      throw Error(
          'Unexpected span in ' + expectedText + ': ' +
          describeSpan(actualSpan) + describeActualSpans()) +
          describeExpectedSpans();
    }
    if (!actualSpan) {
      throw Error(
          'Missing expected span in ' + expectedText + ': ' +
          describeSpan(expectedSpan) + describeActualSpans()) +
          describeExpectedSpans();
    }
    let equal = true;
    if (expectedSpan.start !== actualSpan.start ||
        expectedSpan.end !== actualSpan.end) {
      equal = false;
    } else if (
        expectedSpan.value instanceof OutputNodeSpan &&
        (!(actualSpan.value instanceof OutputNodeSpan) ||
         expectedSpan.value.node !== actualSpan.value.node)) {
      equal = false;
    } else {
      equal =
          (JSON.stringify(expectedSpan.value) ===
           JSON.stringify(actualSpan.value));
    }
    if (!equal) {
      throw Error(
          'Spans differ in this text: "' + expectedText + '":\n' +
          'Expected: ' + describeSpan(expectedSpan) + '\n' +
          'Got     : ' + describeSpan(actualSpan) + describeActualSpans()) +
          describeExpectedSpans();
    }
  }
}

/**
 * Test fixture for output.js.
 */
ChromeVoxOutputE2ETest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    globalThis.Dir = AutomationUtil.Dir;
    globalThis.RoleType = chrome.automation.RoleType;
    this.forceContextualLastOutput();
  }
};


AX_TEST_F('ChromeVoxOutputE2ETest', 'Links', async function() {
  const root = await this.runWithLoadedTree('<a href="#">Click here</a>');
  const el = root.firstChild.firstChild;
  const range = CursorRange.fromNode(el);
  const o = new Output().withSpeechAndBraille(range, null, 'navigate');
  assertEqualsJSON(
      {
        string_: 'Click here|Internal link|Press Search+Space to activate',
        'spans_': [
          // Attributes.
          {value: 'name', start: 0, end: 10},

          // Link earcon (based on the name).
          {value: {earcon: EarconId.LINK}, start: 0, end: 10},

          {value: {'delay': true}, start: 25, end: 55},
        ],
      },
      o.speechOutputForTest);
  checkBrailleOutput(
      'Click here intlnk', [{value: new OutputNodeSpan(el), start: 0, end: 17}],
      o);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'Checkbox', async function() {
  const root = await this.runWithLoadedTree('<input type="checkbox">');
  const el = root.firstChild.firstChild;
  const range = CursorRange.fromNode(el);
  const o = new Output().withSpeechAndBraille(range, null, 'navigate');
  checkSpeechOutput(
      '|Check box|Not checked|Press Search+Space to toggle',
      [
        {value: new OutputEarconAction(EarconId.CHECK_OFF), start: 0, end: 0},
        {value: 'role', start: 1, end: 10},
        {value: {'delay': true}, start: 23, end: 51},
      ],
      o);
  checkBrailleOutput(
      'chk ( )', [{value: new OutputNodeSpan(el), start: 0, end: 7}], o);
});

AX_TEST_F(
    'ChromeVoxOutputE2ETest', 'InLineTextBoxValueGetsIgnored',
    async function() {
      const root = await this.runWithLoadedTree('<p>OK');
      let el = root.firstChild.firstChild.firstChild;
      assertEquals('inlineTextBox', el.role);
      let range = CursorRange.fromNode(el);
      let o = new Output().withSpeechAndBraille(range, null, 'navigate');
      assertEqualsJSON(
          {
            string_: 'OK',
            'spans_': [
              // Attributes.
              {value: 'name', start: 0, end: 2},
            ],
          },
          o.speechOutputForTest);
      checkBrailleOutput(
          'OK', [{value: new OutputNodeSpan(el), start: 0, end: 2}], o);

      el = root.firstChild.firstChild;
      assertEquals('staticText', el.role);
      range = CursorRange.fromNode(el);
      o = new Output().withSpeechAndBraille(range, null, 'navigate');
      assertEqualsJSON(
          {
            string_: 'OK',
            'spans_': [
              // Attributes.
              {value: 'name', start: 0, end: 2},
            ],
          },
          o.speechOutputForTest);
      checkBrailleOutput(
          'OK', [{value: new OutputNodeSpan(el), start: 0, end: 2}], o);
    });

AX_TEST_F('ChromeVoxOutputE2ETest', 'Headings', async function() {
  const root = await this.runWithLoadedTree(`
      <h1>a</h1><h2>b</h2><h3>c</h3><h4>d</h4><h5>e</h5><h6>f</h6>
      <h1><a href="a.com">b</a></h1> `);
  let el = root.firstChild;
  for (let i = 1; i <= 6; ++i) {
    const range = CursorRange.fromNode(el);
    const o = new Output().withSpeechAndBraille(range, null, 'navigate');
    const letter = String.fromCharCode('a'.charCodeAt(0) + i - 1);
    assertEqualsJSON(
        {
          string_: letter + '|Heading ' + i,
          'spans_': [
            // Attributes.
            {value: 'nameOrDescendants', start: 0, end: 1},
          ],
        },
        o.speechOutputForTest);
    checkBrailleOutput(
        letter + ' h' + i, [{value: new OutputNodeSpan(el), start: 0, end: 4}],
        o);
    el = el.nextSibling;
  }

  range = CursorRange.fromNode(el);
  o = new Output().withSpeechAndBraille(range, null, 'navigate');
  assertEqualsJSON(
      {
        string_: 'b|Link|Heading 1',
        'spans_': [
          {value: 'name', start: 0, end: 1},
          {value: new OutputEarconAction(EarconId.LINK), start: 0, end: 1},
          {value: 'role', start: 2, end: 6},
        ],
      },
      o.speechOutputForTest);
  checkBrailleOutput(
      'b lnk h1',
      [
        {value: new OutputNodeSpan(el.firstChild.firstChild), start: 0, end: 1},
        {value: new OutputNodeSpan(el), start: 0, end: 8},
        {value: new OutputNodeSpan(el.firstChild), start: 2, end: 5},
      ],
      o);
});

// TODO(crbug.com/41424286): test is flaky.
AX_TEST_F('ChromeVoxOutputE2ETest', 'DISABLED_Audio', async function() {
  const root =
      await this.runWithLoadedTree('<audio src="foo.mp3" controls></audio>');
  let el = root.find({role: RoleType.BUTTON});
  let range = CursorRange.fromNode(el);
  let o =
      new Output().withoutHints().withSpeechAndBraille(range, null, 'navigate');

  checkSpeechOutput(
      'play|Disabled|Button|audio|Tool bar',
      [
        {value: new OutputEarconAction(EarconId.BUTTON), start: 0, end: 4},
        {value: 'name', start: 21, end: 26},
        {value: 'role', start: 27, end: 35},
      ],
      o);

  checkBrailleOutput(
      'play xx btn audio tlbar',
      [
        {value: new OutputNodeSpan(el), start: 0, end: 11},
        {value: new OutputNodeSpan(el.parent), start: 12, end: 23},
      ],
      o);

  // TODO(dtseng): Replace with a query.
  el = el.nextSibling.nextSibling.nextSibling;
  const prevRange = range;
  range = CursorRange.fromNode(el);
  o = new Output().withoutHints().withSpeechAndBraille(
      range, prevRange, 'navigate');
  checkSpeechOutput(
      '|audio time scrubber|Slider|0:00|Min 0|Max 0',
      [
        {value: 'name', start: 0, end: 0},
        {value: new OutputEarconAction(EarconId.SLIDER), start: 0, end: 0},
        {value: 'description', start: 1, end: 20},
        {value: 'role', start: 21, end: 27},
        {value: 'value', start: 28, end: 32},
      ],
      o);
  checkBrailleOutput(
      'audio time scrubber sldr 0:00 min:0 max:0',
      [{value: new OutputNodeSpan(el), start: 0, end: 41}], o);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'Input', async function() {
  const root = await this.runWithLoadedTree(
      '<input type="text"></input>' +
      '<input type="email"></input>' +
      '<input type="password"></input>' +
      '<input type="tel"></input>' +
      '<input type="number"></input>' +
      '<input type="time"></input>' +
      '<input type="date"></input>' +
      '<input type="file"</input>' +
      '<input type="search"</input>' +
      '<input type="invalidType"</input>');
  const expectedSpansNonSearchBox = [
    {value: 'name', start: 0, end: 0},
    {value: new OutputEarconAction(EarconId.EDITABLE_TEXT), start: 0, end: 0},
    {value: new OutputSelectionSpan(0, 0, 0), start: 1, end: 1},
    {value: 'value', start: 1, end: 1},
    {value: 'inputType', start: 2},
  ];
  const expectedSpansForSearchBox = [
    {value: 'name', start: 0, end: 0},
    {value: new OutputEarconAction(EarconId.EDITABLE_TEXT), start: 0, end: 0},
    {value: new OutputSelectionSpan(0, 0, 0), start: 1, end: 1},
    {value: 'value', start: 1, end: 1},
    {value: 'role', start: 2, end: 8},
  ];

  const expectedSpeechValues = [
    '||Edit text',
    '||Edit text, email entry',
    '||Password edit text',
    '||Edit text numeric only',
    [
      '|Spin button',
      [
        {value: 'name', start: 0, end: 0},
        {value: new OutputEarconAction(EarconId.LISTBOX), start: 0, end: 0},
        {value: 'role', start: 1, end: 12},
      ],
    ],
    ['Time control', [{value: 'role', start: 0, end: 12}]],
    ['Date control', [{value: 'role', start: 0, end: 12}]],
    [
      'Choose File: No file chosen|Button',
      [
        {value: 'name', start: 0, end: 27},
        {value: new OutputEarconAction(EarconId.BUTTON), start: 0, end: 27},
        {value: 'role', start: 28, end: 34},
      ],
    ],
    '||Search',
    '||Edit text',
  ];
  // TODO(plundblad): Some of these are wrong, there should be an initial
  // space for the cursor in edit fields.
  const expectedBrailleValues = [
    ' ed',
    ' @ed 8dot',
    ' pwded',
    ' #ed',
    {string_: 'spnbtn', spans_: []},
    {string_: 'time'},
    {string_: 'date'},
    {string_: 'Choose File: No file chosen btn'},
    ' search',
    ' ed',
  ];
  assertEquals(expectedSpeechValues.length, expectedBrailleValues.length);

  let el = root.firstChild.firstChild;
  expectedSpeechValues.forEach(expectedValue => {
    const range = CursorRange.fromNode(el);
    const o = new Output().withoutHints().withSpeechAndBraille(
        range, null, 'navigate');
    let expectedSpansForValue = null;
    if (typeof expectedValue === 'object') {
      checkSpeechOutput(expectedValue[0], expectedValue[1], o);
    } else {
      expectedSpansForValue = expectedValue === '||Search' ?
          expectedSpansForSearchBox :
          expectedSpansNonSearchBox;
      expectedSpansForValue[4].end = expectedValue.length;
      checkSpeechOutput(expectedValue, expectedSpansForValue, o);
    }
    el = el.nextSibling;
  });

  el = root.firstChild.firstChild;
  expectedBrailleValues.forEach(expectedValue => {
    const range = CursorRange.fromNode(el);
    const o = new Output().withoutHints().withBraille(range, null, 'navigate');
    if (typeof expectedValue === 'string') {
      checkBrailleOutput(
          expectedValue,
          [
            {value: {startIndex: 0, endIndex: 0}, start: 0, end: 0},
            {
              value: new OutputNodeSpan(el),
              start: 0,
              end: expectedValue.length,
            },
          ],
          o);
    } else {
      let spans = [{
        value: new OutputNodeSpan(el),
        start: 0,
        end: expectedValue.string_.length,
      }];
      if (expectedValue.spans_) {
        spans = spans.concat(expectedValue.spans_);
      }

      checkBrailleOutput(expectedValue.string_, spans, o);
    }
    el = el.nextSibling;
  });
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'List', async function() {
  const root = await this.runWithLoadedTree(
      '<ul aria-label="first"><li aria-label="a">a<li>b<li>c</ul>');
  const el = root.firstChild.firstChild;
  const range = CursorRange.fromNode(el);
  const o = new Output().withSpeechAndBraille(range, null, 'navigate');
  checkSpeechOutput(
      'a|List item|first|List|with 3 items',
      [
        {value: {earcon: EarconId.LIST_ITEM}, start: 0, end: 1},
        {value: 'name', start: 12, end: 17},
        {value: 'role', start: 18, end: 22},
      ],
      o);
  // TODO(plundblad): This output is wrong.  Add special handling for
  // braille here.
  checkBrailleOutput(
      'a lstitm first lst +3',
      [
        {value: new OutputNodeSpan(el), start: 0, end: 8},
        {value: new OutputNodeSpan(el.parent), start: 9, end: 21},
      ],
      o);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'ListWithoutSetSize', async function() {
  const root = await this.runWithLoadedTree(
      '<ul aria-label="first"><li aria-label="a">a<li>b<li>c</ul>');
  const el = root.firstChild.firstChild;
  Object.defineProperty(root.firstChild, 'setSize', {get: () => null});

  const range = CursorRange.fromNode(el);
  const o = new Output().withSpeechAndBraille(range, null, 'navigate');

  checkSpeechOutput(
      'a|List item|first|List',
      [
        {value: {earcon: EarconId.LIST_ITEM}, start: 0, end: 1},
        {value: 'name', start: 12, end: 17},
        {value: 'role', start: 18, end: 22},
      ],
      o);
  checkBrailleOutput(
      'a lstitm first lst',
      [
        {value: new OutputNodeSpan(el), start: 0, end: 8},
        {value: new OutputNodeSpan(el.parent), start: 9, end: 18},
      ],
      o);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'Tree', async function() {
  const root = await this.runWithLoadedTree(`
    <ul role="tree" style="list-style-type:none">
      <li aria-expanded="true" role="treeitem">a
      <li role="treeitem">b
      <li aria-expanded="false" role="treeitem">c
    </ul>
  `);
  let el = root.firstChild.children[0].firstChild;
  let range = CursorRange.fromNode(el);
  let o = new Output().withSpeechAndBraille(range, null, 'navigate');
  checkSpeechOutput(
      'a|Tree item|Expanded| 1 of 3 | level 1 |Tree|with 3 items',
      [
        {value: 'name', 'start': 0, end: 1},
        {value: 'state', start: 12, end: 20},
        {value: 'role', 'start': 40, end: 44},
      ],
      o);
  checkBrailleOutput(
      'a tritm - 1/3 level 1 tree +3',
      [
        {value: new OutputNodeSpan(el), start: 0, end: 1},
        {value: new OutputNodeSpan(el.parent), start: 2, end: 22},
        {value: new OutputNodeSpan(el.parent.parent), start: 22, end: 29},
      ],
      o);

  el = root.firstChild.children[1].firstChild;
  range = CursorRange.fromNode(el);
  o = new Output().withSpeechAndBraille(range, null, 'navigate');
  checkSpeechOutput(
      'b|Tree item| 2 of 3 | level 1 |Tree|with 3 items',
      [
        {value: 'name', start: 0, end: 1},
        {value: 'role', 'start': 31, end: 35},
      ],
      o);
  checkBrailleOutput(
      'b tritm 2/3 level 1 tree +3',
      [
        {value: new OutputNodeSpan(el), start: 0, end: 1},
        {value: new OutputNodeSpan(el.parent), start: 2, end: 20},
        {value: new OutputNodeSpan(el.parent.parent), start: 20, end: 27},
      ],
      o);

  el = root.firstChild.children[2].firstChild;
  range = CursorRange.fromNode(el);
  o = new Output().withSpeechAndBraille(range, null, 'navigate');
  checkSpeechOutput(
      'c|Tree item|Collapsed| 3 of 3 | level 1 |Tree|with 3 items',
      [
        {value: 'name', 'start': 0, end: 1},
        {value: 'state', start: 12, end: 21},
        {value: 'role', 'start': 41, end: 45},
      ],
      o);
  checkBrailleOutput(
      'c tritm + 3/3 level 1 tree +3',
      [
        {value: new OutputNodeSpan(el), start: 0, end: 1},
        {value: new OutputNodeSpan(el.parent), start: 2, end: 22},
        {value: new OutputNodeSpan(el.parent.parent), start: 22, end: 29},
      ],
      o);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'Menu', async function() {
  const site = `
    <div role="menu">
      <div role="menuitem">a</div>
      <div role="menuitemcheckbox">b</div>
      <div role="menuitemradio">c</div>
    </div>
    <div role="menubar" aria-orientation="horizontal"></div>
  `;
  const root = await this.runWithLoadedTree(site);
  let el = root.firstChild.firstChild;
  let range = CursorRange.fromNode(el);
  let o = new Output().withSpeechAndBraille(range, null, 'navigate');
  checkSpeechOutput(
      'a|Menu item| 1 of 3 |Menu',
      [{value: 'name', start: 0, end: 1}, {value: 'role', start: 21, end: 25}],
      o);
  checkBrailleOutput(
      'a mnuitm 1/3 mnu',
      [
        {value: new OutputNodeSpan(el), start: 0, end: 12},
        {value: new OutputNodeSpan(el.parent), start: 13, end: 16},
      ],
      o);

  // Ancestry.
  el = root.firstChild;
  range = CursorRange.fromNode(el);
  o = new Output().withSpeechAndBraille(range, null, 'navigate');
  checkSpeechOutput(
      'Menu|with 3 items|' +
          'Press up or down arrow to navigate; enter to activate',
      [
        {value: 'role', start: 0, end: 4},
        {value: {delay: true}, start: 18, end: 71},
      ],
      o);

  el = root.lastChild;
  range = CursorRange.fromNode(el);
  o = new Output().withSpeechAndBraille(range, null, 'navigate');
  checkSpeechOutput(
      'Menu bar|Press left or right arrow to navigate; enter to activate',
      [
        {value: 'role', start: 0, end: 8},
        {value: {delay: true}, start: 9, end: 65},
      ],
      o);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'ListBox', async function() {
  const root = await this.runWithLoadedTree(`
    <select multiple>
      <option>1</option>
      <option>2</option>
    </select>
  `);
  const el = root.firstChild.firstChild.firstChild;
  const range = CursorRange.fromNode(el);
  const o = new Output().withSpeechAndBraille(range, null, 'navigate');
  checkSpeechOutput(
      '1|List item| 1 of 2 |Not selected|List box|with 2 items',
      [
        {value: 'name', start: 0, end: 1},
        {value: new OutputEarconAction(EarconId.LIST_ITEM), start: 0, end: 1},
        {value: 'role', start: 34, end: 42},
      ],
      o);
  checkBrailleOutput(
      '1 lstitm 1/2 ( ) lstbx +2',
      [
        {value: new OutputNodeSpan(el), start: 0, end: 16},
        {value: new OutputNodeSpan(el.parent), start: 17, end: 25},
      ],
      o);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'MessageIdAndEarconValidity', function() {
  const kNoBrailleMessageRequired = new Set([
    'comment',
    'contentDeletion',
    'contentInsertion',
    'docAbstract',
    'docAcknowledgments',
    'docAfterword',
    'docAppendix',
    'docBackLink',
    'docBiblioEntry',
    'docBibliography',
    'docBiblioRef',
    'docChapter',
    'docColophon',
    'docConclusion',
    'docCover',
    'docCredit',
    'docCredits',
    'docDedication',
    'docEndnote',
    'docEndnotes',
    'docEpigraph',
    'docEpilogue',
    'docErrata',
    'docExample',
    'docFootnote',
    'docForeword',
    'docGlossary',
    'docGlossRef',
    'docIndex',
    'docIntroduction',
    'docNoteRef',
    'docNotice',
    'docPageBreak',
    'docPageFooter',
    'docPageHeader',
    'docPageList',
    'docPart',
    'docPreface',
    'docPrologue',
    'docPullquote',
    'docQna',
    'docSubtitle',
    'docTip',
    'docToc',
    'graphicsDocument',
    'graphicsObject',
    'graphicsSymbol',
    'suggestion',
  ]);
  for (const key in OutputRoleInfo) {
    const value = OutputRoleInfo[key];
    if (value.msgId) {
      Msgs.getMsg(value.msgId);
      if (!kNoBrailleMessageRequired.has(key)) {
        Msgs.getMsg(value.msgId + '_brl');
      }
      assertFalse(/[A-Z]+/.test(value.msgId));
    }
  }
  for (const key in Output.STATE_INFO_) {
    const value = Output.STATE_INFO_[key];
    for (innerKey in value) {
      const innerValue = value[innerKey];
      if (typeof (innerValue) === 'boolean') {
        assertEquals('isRoleSpecific', innerKey);
        continue;
      }
      Msgs.getMsg(innerValue.msgId);
      Msgs.getMsg(innerValue.msgId + '_brl');
      assertFalse(/[A-Z]+/.test(innerValue.msgId));
    }
  }
  for (const key in Output.INPUT_TYPE_MESSAGE_IDS_) {
    const msgId = Output.INPUT_TYPE_MESSAGE_IDS_[key];
    assertFalse(/[A-Z]+/.test(msgId));
    Msgs.getMsg(msgId);
    Msgs.getMsg(msgId + '_brl');
  }
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'DivOmitsRole', async function() {
  const root = await this.runWithLoadedTree(`
    <div>that has content</div>
    <div></div>
    <div role='group'><div>nested content</div></div>
  `);
  const el = root.firstChild.firstChild;
  const range = CursorRange.fromNode(el);
  const o = new Output().withSpeechAndBraille(range, null, 'navigate');
  checkSpeechOutput(
      'that has content', [{value: 'name', start: 0, end: 16}], o);
  checkBrailleOutput(
      'that has content', [{value: new OutputNodeSpan(el), start: 0, end: 16}],
      o);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'LessVerboseAncestry', async function() {
  const root = await this.runWithLoadedTree(`
    <div role="banner"><p>inside</p></div>
    <div role="banner"><p>inside</p></div>
    <div role="navigation"><p>inside</p></div>
  `);
  const first = root.children[0].firstChild;
  const second = root.children[1].firstChild;
  const third = root.children[2].firstChild;
  const firstRange = CursorRange.fromNode(first);
  const secondRange = CursorRange.fromNode(second);
  const thirdRange = CursorRange.fromNode(third);

  const oWithoutPrev = new Output().withSpeech(firstRange, null, 'navigate');
  const oWithPrev =
      new Output().withSpeech(secondRange, firstRange, 'navigate');
  const oWithPrevExit =
      new Output().withSpeech(thirdRange, secondRange, 'navigate');
  assertEquals('inside|Banner', oWithoutPrev.speechOutputForTest.string_);

  // Make sure we don't read the exited ancestry change.
  assertEquals('inside|Banner', oWithPrev.speechOutputForTest.string_);

  // Different role; do read the exited ancestry here.
  assertEquals('inside|Navigation', oWithPrevExit.speechOutputForTest.string_);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'Brief', async function() {
  const root = await this.runWithLoadedTree(`
    <div role="article"><p>inside</p></div>
  `);
  const node = root.children[0].firstChild;
  const range = CursorRange.fromNode(node);

  SettingsManager.set('useVerboseMode', false);
  const oWithoutPrev = new Output().withSpeech(range, null, 'navigate');
  assertEquals('inside', oWithoutPrev.speechOutputForTest.string_);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'AuralStyledHeadings', async function() {
  function toFixed(num) {
    return parseFloat(Number(num).toFixed(1));
  }
  const root = await this.runWithLoadedTree(`
      <h1>a</h1><h2>b</h2><h3>c</h3><h4>d</h4><h5>e</h5><h6>f</h6>
      <h1><a href="a.com">b</a></h1> `);
  let el = root.firstChild;
  for (let i = 1; i <= 6; ++i) {
    const range = CursorRange.fromNode(el);
    const o = new Output().withRichSpeech(range, null, 'navigate');
    const letter = String.fromCharCode('a'.charCodeAt(0) + i - 1);
    assertEqualsJSON(
        {
          string_: letter + '|Heading ' + i,
          'spans_': [
            // Aural styles.
            {value: {'relativePitch': toFixed(-0.1 * i)}, start: 0, end: 0},

            // Attributes.
            {value: 'nameOrDescendants', start: 0, end: 1},

            {value: {'relativePitch': -0.2}, start: 2, end: 2},
          ],
        },
        o.speechOutputForTest);
    el = el.nextSibling;
  }
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'ToggleButton', async function() {
  const root = await this.runWithLoadedTree(`
      <div role="button" aria-pressed="true">Subscribe</div>`);
  const el = root.firstChild;
  const o = new Output().withSpeechAndBraille(CursorRange.fromNode(el));
  assertEqualsJSON(
      {
        string_:
            '|Subscribe|Toggle Button|Pressed|Press Search+Space to toggle',
        spans_: [
          {value: {earcon: EarconId.CHECK_ON}, start: 0, end: 0},
          {value: 'name', start: 1, end: 10},
          {value: 'role', start: 11, end: 24},
          {value: {'delay': true}, start: 33, end: 61},
        ],
      },
      o.speechOutputForTest);
  assertEquals('Subscribe tgl btn =', o.braille.string_);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'JoinDescendants', async function() {
  const root = await this.runWithLoadedTree(`
      <p>This</p>
      <p>fragment</p>
      <p>Should be separated</p>
      <p>with spaces</p>
    `);
  const unjoined = new Output().format('$descendants', root);
  assertEquals(
      'This|fragment|Should be separated|with spaces',
      unjoined.speechOutputForTest.string_);

  const joined = new Output().format('$joinedDescendants', root);
  assertEquals(
      'This fragment Should be separated with spaces',
      joined.speechOutputForTest.string_);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'ComplexDiv', async function() {
  const root = await this.runWithLoadedTree(`
      <div><button>ok</button></div>
    `);
  const div = root.find({role: RoleType.GENERIC_CONTAINER});
  const o = new Output().withSpeech(CursorRange.fromNode(div));
  assertEquals('ok', o.speechOutputForTest.string_);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'ContainerFocus', async function() {
  const root = await this.runWithLoadedTree(`
      <div role="grid">
        <div role="row" tabindex=0 aria-label="start"></div>
        <div role="row" tabindex=0 aria-label="end"></div>
      </div>
    `);
  const r1 = CursorRange.fromNode(root.firstChild.firstChild);
  const r2 = CursorRange.fromNode(root.firstChild.lastChild);
  assertEquals(
      'start|Row', new Output().withSpeech(r1, r2).speechOutputForTest.string_);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'BraileWhitespace', async function() {
  const root = await this.runWithLoadedTree(`
    <p>this is a <em>test</em>of emphasized text</p>
  `);
  const start = root.firstChild.firstChild;
  const end = root.firstChild.lastChild;
  const range = new CursorRange(Cursor.fromNode(start), Cursor.fromNode(end));
  const o = new Output().withBraille(range, null, 'navigate');
  checkBrailleOutput(
      'this is a test of emphasized text',
      [
        {value: new OutputNodeSpan(start), start: 0, end: 10},
        {
          value: new OutputNodeSpan(start.nextSibling.firstChild),
          start: 10,
          end: 14,
        },
        {value: new OutputNodeSpan(end), start: 15, end: 33},
      ],
      o);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'BrailleAncestry', async function() {
  const root = await this.runWithLoadedTree(`
    <ul><li><a href="#">test</a></li></ul>
  `);
  const link = root.find({role: RoleType.LINK});
  // The 'inlineTextBox' found from root would return the inlineTextBox of
  // the list marker. Here we want the link's inlineTextBox.
  const text = link.find({role: RoleType.INLINE_TEXT_BOX});
  const listItem = root.find({role: RoleType.LIST_ITEM});
  const list = root.find({role: RoleType.LIST});
  let range = CursorRange.fromNode(text);
  let o = new Output().withBraille(range, null, 'navigate');
  checkBrailleOutput(
      'test lnk lstitm lst end',
      [
        {value: new OutputNodeSpan(text), start: 0, end: 4},
        {value: new OutputNodeSpan(link), start: 5, end: 8},
        {value: new OutputNodeSpan(listItem), start: 9, end: 15},
        {value: new OutputNodeSpan(list), start: 16, end: 23},
      ],
      o);

  // Now, test the "bullet" which comes before the above.
  const bullet = root.find({role: RoleType.LIST_MARKER});
  range = CursorRange.fromNode(bullet);
  o = new Output().withBraille(range, null, 'navigate');
  checkBrailleOutput(
      '\u2022 lstitm lst +1',
      [
        {value: new OutputNodeSpan(bullet), start: 0, end: 2},
        {value: new OutputNodeSpan(listItem), start: 2, end: 8},
        {value: new OutputNodeSpan(list), start: 9, end: 15},
      ],
      o);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'RangeOutput', async function() {
  const root = await this.runWithLoadedTree(`
    <div role="slider" aria-valuemin="1" aria-valuemax="10" aria-valuenow="2"
                       aria-label="volume"></div>
    <progress aria-valuemin="1" aria-valuemax="10"
              aria-valuenow="2" aria-label="volume"></progress>
    <meter aria-valuemin="1" aria-valuemax="10" aria-valuenow="2"
           aria-label="volume"></meter>
    <div role="spinbutton" aria-valuemin="1" aria-valuemax="10"
                           aria-valuenow="2" aria-label="volume"></div>
  `);
  let obj = root.find({role: RoleType.SLIDER});
  let o = new Output().withoutHints().withSpeech(CursorRange.fromNode(obj));
  checkSpeechOutput(
      'volume|Slider|2|Min 1|Max 10',
      [
        {value: 'name', start: 0, end: 6},
        {value: new OutputEarconAction(EarconId.SLIDER), start: 0, end: 6},
        {value: 'role', start: 7, end: 13},
        {value: 'value', start: 14, end: 15},
      ],
      o);

  obj = root.find({role: RoleType.PROGRESS_INDICATOR});
  o = new Output().withoutHints().withSpeech(CursorRange.fromNode(obj));
  checkSpeechOutput(
      'volume|Progress indicator|2|Min 1|Max 10',
      [
        {value: 'name', start: 0, end: 6},
        {value: 'role', start: 7, end: 25},
        {value: 'value', start: 26, end: 27},
      ],
      o);

  obj = root.find({role: RoleType.METER});
  o = new Output().withoutHints().withSpeech(CursorRange.fromNode(obj));
  checkSpeechOutput(
      'volume|Meter|2|Min 1|Max 10',
      [
        {value: 'name', start: 0, end: 6},
        {value: 'role', start: 7, end: 12},
        {value: 'value', start: 13, end: 14},
      ],
      o);

  obj = root.find({role: RoleType.SPIN_BUTTON});
  o = new Output().withoutHints().withSpeech(CursorRange.fromNode(obj));
  checkSpeechOutput(
      'volume|Spin button|2|Min 1|Max 10',
      [
        {value: 'name', start: 0, end: 6},
        {value: new OutputEarconAction(EarconId.LISTBOX), start: 0, end: 6},
        {value: 'role', start: 7, end: 18},
        {value: 'value', start: 19, end: 20},
      ],
      o);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'RoleDescription', async function() {
  const root = await this.runWithLoadedTree(`
    <div aria-label="hi" role="button" aria-roledescription="foo"></div>
  `);
  const obj = root.find({role: RoleType.BUTTON});
  const o = new Output().withoutHints().withSpeech(CursorRange.fromNode(obj));
  checkSpeechOutput(
      'hi|foo',
      [
        {value: 'name', start: 0, end: 2},
        {value: new OutputEarconAction(EarconId.BUTTON), start: 0, end: 2},
        {value: 'role', start: 3, end: 6},
      ],
      o);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'ValidateCommonProperties', function() {
  // If you fail this test, you likely need to insert a $state, $restriction or
  // $description into the output rules for the printed roles. Typically,
  // $description goes towards the end of the output rule, though this depends
  // on the role. For example, it could make sense to put $description before
  // $value or $state.

  // You can also add the role to be excluded from this check. You are
  // encouraged to write a more intelligent output rule to provide friendlier
  // feedback. For example, 'not selected apple item 2 out of 3' coming from a
  // message template like '@smart_selection($state, $name, $indexInParent,
  // $childCount)'.
  // In such cases, you are responsible for ensuring you include all states and
  // descriptions somewhere in the output.

  const stateStr = '$state';
  const restrictionStr = '$restriction';
  const descStr = '$description';
  let missingState = [];
  let missingRestriction = [];
  let missingDescription = [];
  for (const key in OutputRule.RULES.navigate) {
    const speak = OutputRule.RULES.navigate[key].speak;
    if (!speak) {
      continue;
    }

    if (speak.indexOf(stateStr) === -1) {
      missingState.push(key);
    }
    if (speak.indexOf(restrictionStr) === -1) {
      missingRestriction.push(key);
    }
    if (speak.indexOf(descStr) === -1) {
      missingDescription.push(key);
    }
  }

  // This filters out known roles that don't have states or descriptions.
  const notStated = [
    RoleType.CLIENT,
    RoleType.EMBEDDED_OBJECT,
    RoleType.IME_CANDIDATE,
    RoleType.INLINE_TEXT_BOX,
    RoleType.LINE_BREAK,
    RoleType.LIST_MARKER,
    RoleType.PARAGRAPH,
    RoleType.ROOT_WEB_AREA,
    RoleType.STATIC_TEXT,
    RoleType.PLUGIN_OBJECT,
    RoleType.WINDOW,
  ];
  const notRestricted = [
    RoleType.ALERT,
    RoleType.ALERT_DIALOG,
    RoleType.CELL,
    RoleType.CLIENT,
    RoleType.EMBEDDED_OBJECT,
    RoleType.GENERIC_CONTAINER,
    RoleType.GRID_CELL,
    RoleType.IMAGE,
    RoleType.IME_CANDIDATE,
    RoleType.INLINE_TEXT_BOX,
    RoleType.LINE_BREAK,
    RoleType.LIST,
    RoleType.LIST_MARKER,
    RoleType.PARAGRAPH,
    RoleType.PLUGIN_OBJECT,
    RoleType.REGION,
    RoleType.ROOT_WEB_AREA,
    RoleType.ROW_HEADER,
    RoleType.STATIC_TEXT,
    RoleType.TABLE_HEADER_CONTAINER,
    RoleType.TIMER,
    RoleType.WINDOW,
  ];
  const notDescribed = [
    RoleType.CLIENT,
    RoleType.EMBEDDED_OBJECT,
    RoleType.IME_CANDIDATE,
    RoleType.INLINE_TEXT_BOX,
    RoleType.LINE_BREAK,
    RoleType.LIST_MARKER,
    RoleType.PARAGRAPH,
    RoleType.PLUGIN_OBJECT,
    RoleType.ROOT_WEB_AREA,
    RoleType.STATIC_TEXT,
    RoleType.WINDOW,
  ];
  missingState = missingState.filter(state => notStated.indexOf(state) === -1);
  missingRestriction = missingRestriction.filter(
      restriction => notRestricted.indexOf(restriction) === -1);
  missingDescription =
      missingDescription.filter(desc => notDescribed.indexOf(desc) === -1);

  assertEquals(
      0, missingState.length,
      'Unexpected missing states for output rules ' + missingState.join(' '));
  assertEquals(
      0, missingRestriction.length,
      'Unexpected missing restriction for output rules ' +
          missingRestriction.join(' '));
  assertEquals(
      0, missingDescription.length,
      'Unexpected missing descriptions for output rules ' +
          missingDescription.join(' '));
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'ValidateRoles', function() {
  // If you fail this test, you likely need to insert a $role or
  // $roledescription into the output rules for the printed roles. Typically,
  // roles can be omitted (intentionally), but role descriptions cannot by W3C
  // spec (valid on all base markup). However, not all roles come from the web.
  //
  // Some enter rules do not need roles, depending on how they are used such as
  // if their speak rule is always triggered during navigation e.g. buttons.
  //
  // You can also add the role to be excluded from this check. You are
  // encouraged to write a more intelligent output rule to provide friendlier
  // feedback, but keep in mind role descriptions are required on all web-based
  // role.

  const roleOrRoleDescStr = '$role';
  const missingRole = [];
  const allowedMissingRoles = [
    RoleType.CLIENT,
    RoleType.GENERIC_CONTAINER,
    RoleType.EMBEDDED_OBJECT,
    RoleType.IME_CANDIDATE,
    RoleType.INLINE_TEXT_BOX,
    RoleType.LINE_BREAK,
    RoleType.LIST_MARKER,
    RoleType.ROOT_WEB_AREA,
    RoleType.STATIC_TEXT,
    RoleType.WINDOW,
  ];
  for (const key in OutputRule.RULES.navigate) {
    if (allowedMissingRoles.indexOf(key) !== -1) {
      continue;
    }
    const speak = OutputRule.RULES.navigate[key].speak;
    let enter = OutputRule.RULES.navigate[key].enter;
    if (enter && enter.speak) {
      enter = enter.speak;
    }

    if (speak && speak.indexOf(roleOrRoleDescStr) === -1) {
      missingRole.push(key);
    }
    if (enter && enter.indexOf(roleOrRoleDescStr) === -1) {
      missingRole.push(key);
    }
  }
  assertEquals(
      0, missingRole.length,
      'Unexpected missing role or role description for output rules ' +
          missingRole.join(' '));
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'InlineBraille', async function() {
  const root = await this.runWithLoadedTree(`
    <table border=1>
      <tr><td>Name</td><td id="active">Age</td><td>Address</td></tr>
    </table>
  `);
  const obj = root.find({role: RoleType.CELL});
  const o = new Output().withRichSpeechAndBraille(CursorRange.fromNode(obj));
  assertEquals(
      'Name|row 1 column 1|Table , 1 by 3', o.speechOutputForTest.string_);
  assertEquals('Name r1c1 Age r1c2 Address r1c3', o.braille.string_);
});

AX_TEST_F(
    'ChromeVoxOutputE2ETest', 'TextFieldObeysRoleDescription',
    async function() {
      const root = await this.runWithLoadedTree(`
    <div role="textbox" aria-roledescription="square"></div>
    <div role="region" aria-roledescription="circle"></div>
  `);
      const text = root.find({role: RoleType.TEXT_FIELD});

      // True even though |text| does not have editable state.
      assertTrue(AutomationPredicate.editText(text));

      let o = new Output().withRichSpeechAndBraille(CursorRange.fromNode(text));
      assertEquals('|square', o.speechOutputForTest.string_);
      assertEquals('square', o.braille.string_);

      const region = root.find({role: RoleType.REGION});
      o = new Output().withRichSpeechAndBraille(CursorRange.fromNode(region));
      assertEquals('circle', o.speechOutputForTest.string_);
      assertEquals('circle', o.braille.string_);
    });

AX_TEST_F('ChromeVoxOutputE2ETest', 'NestedList', async function() {
  const root = await this.runWithLoadedTree(`
    <ul role="tree">schedule
      <li role="treeitem">wake up
      <li role="treeitem">drink coffee
      <ul role="tree">tasks
        <li role="treeitem" aria-level='2'>meeting
        <li role="treeitem" aria-level='2'>lunch
      </ul>
      <li role="treeitem">cook dinner
    </ul>
  `);
  const lists = root.findAll({role: RoleType.TREE});
  const outerList = lists[0];
  const innerList = lists[1];

  let el = outerList.children[0];
  let startRange = CursorRange.fromNode(el);
  let o = new Output().withSpeech(startRange, null, 'navigate');
  assertEquals('schedule|Tree|with 3 items', o.speechOutputForTest.string_);

  el = outerList.children[1];
  startRange = CursorRange.fromNode(el);
  o = new Output().withSpeech(
      startRange, CursorRange.fromNode(outerList.children[0]), 'navigate');
  assertEquals(
      'wake up|Tree item|Not selected| 1 of 3 | level 1 ',
      o.speechOutputForTest.string_);

  el = outerList.children[2];
  startRange = CursorRange.fromNode(el);
  o = new Output().withSpeech(
      startRange, CursorRange.fromNode(outerList.children[0]), 'navigate');
  assertEquals(
      'drink coffee|Tree item|Not selected| 2 of 3 | level 1 ',
      o.speechOutputForTest.string_);

  el = outerList.children[3];
  startRange = CursorRange.fromNode(el);
  o = new Output().withSpeech(
      startRange, CursorRange.fromNode(outerList.children[0]), 'navigate');
  assertEquals(
      'cook dinner|Tree item|Not selected| 3 of 3 | level 1 ',
      o.speechOutputForTest.string_);

  el = innerList.children[0];
  startRange = CursorRange.fromNode(el);
  o = new Output().withSpeech(
      startRange, CursorRange.fromNode(outerList.children[2]), 'navigate');
  assertEquals('tasks|Tree|with 2 items', o.speechOutputForTest.string_);

  el = innerList.children[1];
  startRange = CursorRange.fromNode(el);
  o = new Output().withSpeech(
      startRange, CursorRange.fromNode(innerList.children[0]), 'navigate');
  assertEquals(
      'meeting|Tree item|Not selected| 1 of 2 | level 2 ',
      o.speechOutputForTest.string_);

  el = innerList.children[2];
  startRange = CursorRange.fromNode(el);
  o = new Output().withSpeech(
      startRange, CursorRange.fromNode(innerList.children[0]), 'navigate');
  assertEquals(
      'lunch|Tree item|Not selected| 2 of 2 | level 2 ',
      o.speechOutputForTest.string_);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'NoTooltipWithNameTitle', async function() {
  const root = await this.runWithLoadedTree(`
    <div role="group" title="title"></div>
    <div role="group" aria-label="label" title="title"></div>
    <div role="group" aria-describedby="desc" title="title"></div>
    <div role="group" aria-label="label" aria-describedby="desc" title="title">
    </div>
    <div role="group" aria-label=""></div>
    <p id="desc">describedby</p>
  `);
  const title = root.children[0];
  let o =
      new Output().withSpeech(CursorRange.fromNode(title), null, 'navigate');
  assertEqualsJSON(
      {
        string_: 'title',
        spans_: [
          {value: 'nameOrDescendants', start: 0, end: 5},
        ],
      },
      o.speechOutputForTest);

  const labelTitle = root.children[1];
  o = new Output().withSpeech(
      CursorRange.fromNode(labelTitle), null, 'navigate');
  assertEqualsJSON(
      {
        string_: 'label|title',
        spans_: [
          {value: 'nameOrDescendants', start: 0, end: 5},
          {value: 'description', start: 6, end: 11},
        ],
      },
      o.speechOutputForTest);

  const describedByTitle = root.children[2];
  o = new Output().withSpeech(
      CursorRange.fromNode(describedByTitle), null, 'navigate');
  assertEqualsJSON(
      {
        string_: 'title|describedby',
        spans_: [
          {value: 'nameOrDescendants', start: 0, end: 5},
          {value: 'description', start: 6, end: 17},
        ],
      },
      o.speechOutputForTest);

  const labelDescribedByTitle = root.children[3];
  o = new Output().withSpeech(
      CursorRange.fromNode(labelDescribedByTitle), null, 'navigate');
  assertEqualsJSON(
      {
        string_: 'label|describedby',
        spans_: [
          {value: 'nameOrDescendants', start: 0, end: 5},
          {value: 'description', start: 6, end: 17},
        ],
      },
      o.speechOutputForTest);

  // Hijack the 4th node to force tooltip to return a value. This can only
  // occur on ARC++ where tooltip gets set even if name and description
  // are both empty.
  const tooltip = root.children[4];
  Object.defineProperty(root.children[4], 'tooltip', {get: () => 'tooltip'});

  o = new Output().withSpeech(CursorRange.fromNode(tooltip), null, 'navigate');
  assertEqualsJSON(
      {
        string_: 'tooltip',
        spans_: [{value: {'delay': true}, start: 0, end: 7}],
      },
      o.speechOutputForTest);
});

AX_TEST_F(
    'ChromeVoxOutputE2ETest', 'InitialSpeechProperties', async function() {
      const root = await this.runWithLoadedTree(`
    <p>test</p>  `);
      // Capture speech properties sent to tts.
      this.currentProperties = [];
      ChromeVox.tts.speak = (textString, queueMode, properties) => {
        this.currentProperties.push(properties);
      };

      const o = new Output().withSpeech(CursorRange.fromNode(root.firstChild));
      o.go();
      assertEqualsJSON([{category: TtsCategory.NAV}], this.currentProperties);
      this.currentProperties = [];

      o.withInitialSpeechProperties({
        phoneticCharacters: true,
        // This should not override existing value.
        category: TtsCategory.LIVE,
      });
      o.go();
      assertEqualsJSON(
          [{phoneticCharacters: true, category: TtsCategory.NAV}],
          this.currentProperties);
    });

AX_TEST_F('ChromeVoxOutputE2ETest', 'NameOrTextContent', async function() {
  const root = await this.runWithLoadedTree(`
        <div tabindex=0>
          <div aria-label="hello there world">
            <p>hello world</p>
          </div>
        </div>
      `);
  const focusableDiv = root.firstChild;
  assertEquals(RoleType.GENERIC_CONTAINER, focusableDiv.role);
  assertEquals(chrome.automation.NameFromType.CONTENTS, focusableDiv.nameFrom);
  const o = new Output().withSpeech(CursorRange.fromNode(focusableDiv));
  assertEquals('hello there world', o.speechOutputForTest.string_);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'AriaCurrentHint', async function() {
  const site = `
      <div aria-current="page">Home</div>
      <div aria-current="false">About</div>
      `;
  const root = await this.runWithLoadedTree(site);
  const currentDiv = root.firstChild;
  assertEquals(
      chrome.automation.AriaCurrentState.PAGE, currentDiv.ariaCurrentState);
  const o = new Output().withSpeech(
      CursorRange.fromNode(currentDiv), null, 'navigate');
  assertEquals('Home|Current page', o.speechOutputForTest.string_);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'DelayHintVariants', async function() {
  const root = await this.runWithLoadedTree(`
    <div aria-errormessage="error" aria-invalid="true">OK</div>
    <div id="error" aria-label="error"></div>
  `);
  const div = root.children[0];
  const range = CursorRange.fromNode(div);

  let o = new Output().withSpeech(range, null, 'navigate');
  assertEqualsJSON(
      {string_: 'OK|error', spans_: [{value: 'name', start: 3, end: 8}]},
      o.speechOutputForTest);

  // Force a few properties to be set so that hints are triggered.
  Object.defineProperty(div, 'clickable', {get: () => true});

  o = new Output().withSpeech(range, null, 'navigate');
  assertEqualsJSON(
      {
        string_: 'OK|error|Press Search+Space to activate',
        spans_: [
          {value: 'name', start: 3, end: 8},
          {value: {delay: true}, start: 9, end: 39},
        ],
      },
      o.speechOutputForTest);

  Object.defineProperty(div, 'placeholder', {get: () => 'placeholder'});
  o = new Output().withSpeech(range, null, 'navigate');
  assertEqualsJSON(
      {
        string_: 'OK|error|placeholder|Press Search+Space to activate',
        spans_: [
          {value: 'name', start: 3, end: 8},
          {value: {delay: true}, start: 9, end: 20},
          {start: 21, end: 51},
        ],
      },
      o.speechOutputForTest);
});

AX_TEST_F(
    'ChromeVoxOutputE2ETest', 'DelayHintWithActionLabel', async function() {
      const site = `<button>OK</button>`;
      const root = await this.runWithLoadedTree(site);
      const button = root.children[0];
      const range = CursorRange.fromNode(button);

      let o = new Output().withSpeech(range, null, 'navigate');

      // Force a few properties to be set so that hints are triggered.
      Object.defineProperty(button, 'clickable', {get: () => true});

      Object.defineProperty(
          button, 'doDefaultLabel', {get: () => 'click label'});
      o = new Output().withSpeech(range, null, 'navigate');
      assertEqualsJSON(
          {
            string_: 'OK|Press Search+Space to click label',
            spans_: [{value: {delay: true}, start: 3, end: 36}],
          },
          o.speechOutputForTest);
    });

AX_TEST_F(
    'ChromeVoxOutputE2ETest', 'DelayHintVariantsWithTouch', async function() {
      const site = `<button>OK</button>`;
      const root = await this.runWithLoadedTree(site);
      const button = root.children[0];
      const range = CursorRange.fromNode(button);

      // Force a few properties to be set so that hints are triggered.
      Object.defineProperty(button, 'clickable', {get: () => true});
      EventSource.set(EventSourceType.TOUCH_GESTURE);

      o = new Output().withSpeech(range, null, 'navigate');
      assertEqualsJSON(
          {
            string_: 'OK|Double tap to activate',
            spans_: [
              {value: {delay: true}, start: 3, end: 25},
            ],
          },
          o.speechOutputForTest);

      Object.defineProperty(button, 'doDefaultLabel', {get: () => 'tap label'});

      o = new Output().withSpeech(range, null, 'navigate');
      assertEqualsJSON(
          {
            string_: 'OK|Double tap to tap label',
            spans_: [{value: {delay: true}, start: 3, end: 26}],
          },
          o.speechOutputForTest);
    });

AX_TEST_F(
    'ChromeVoxOutputE2ETest', 'DelayHintWithLongClickLabel', async function() {
      const site = `<button>OK</button>`;
      const root = await this.runWithLoadedTree(site);
      const button = root.children[0];
      const range = CursorRange.fromNode(button);

      let o = new Output().withSpeech(range, null, 'navigate');

      // Force a few properties to be set so that hints are triggered.
      Object.defineProperty(button, 'longClickable', {get: () => true});
      Object.defineProperty(
          button, 'longClickLabel', {get: () => 'long click label'});

      o = new Output().withSpeech(range, null, 'navigate');
      assertEqualsJSON(
          {
            string_: 'OK|Press Search+Shift+Space to long click label',
            spans_: [{value: {delay: true}, start: 3, end: 47}],
          },
          o.speechOutputForTest);
    });

AX_TEST_F(
    'ChromeVoxOutputE2ETest', 'DelayHintLongClickFollowsClick',
    async function() {
      const site = `<button>OK</button>`;
      const root = await this.runWithLoadedTree(site);
      const button = root.children[0];
      const range = CursorRange.fromNode(button);

      let o = new Output().withSpeech(range, null, 'navigate');

      // Force a few properties to be set so that hints are triggered.
      Object.defineProperty(button, 'clickable', {get: () => true});
      Object.defineProperty(button, 'longClickable', {get: () => true});

      o = new Output().withSpeech(range, null, 'navigate');
      assertEqualsJSON(
          {
            string_: 'OK|Press Search+Space to activate|' +
                'Press Search+Shift+Space to long click',
            spans_: [
              {value: {delay: true}, start: 3, end: 33},
              {start: 34, end: 72},
            ],
          },
          o.speechOutputForTest);
    });

AX_TEST_F('ChromeVoxOutputE2ETest', 'WithoutFocusRing', async function() {
  const site = `<button></button>`;
  const root = await this.runWithLoadedTree(site);
  let called = false;
  FocusBounds.set = this.newCallback(() => {
    called = true;
  });

  const button = root.find({role: RoleType.BUTTON});

  // Triggers drawing of the focus ring.
  new Output().withSpeech(CursorRange.fromNode(button)).go();
  assertTrue(called);
  called = false;

  // Does not trigger drawing of the focus ring.
  new Output().withSpeech(CursorRange.fromNode(button)).withoutFocusRing().go();
  assertFalse(called);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'ARCCheckbox', async function() {
  const root = await this.runWithLoadedTree('<input type="checkbox">');
  const checkbox = root.firstChild.firstChild;

  Object.defineProperty(
      checkbox, 'checkedStateDescription',
      {get: () => 'checked state description'});
  const range = CursorRange.fromNode(checkbox);
  const o =
      new Output().withoutHints().withSpeechAndBraille(range, null, 'navigate');
  checkSpeechOutput(
      '|Check box|checked state description',
      [
        {value: new OutputEarconAction(EarconId.CHECK_OFF), start: 0, end: 0},
        {value: 'role', start: 1, end: 10},
        {value: 'checkedStateDescription', start: 11, end: 36},
      ],
      o);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'ARCCustomAction', async function() {
  const root = await this.runWithLoadedTree('<p>test</p>');
  const actionable = root.firstChild.firstChild;
  Object.defineProperty(actionable, 'customActions', {
    get: () => [{id: 0, description: 'custom action description'}],
  });
  const range = CursorRange.fromNode(actionable);
  const o = new Output().withSpeechAndBraille(range, null, 'navigate');
  checkSpeechOutput(
      'test|Actions available. Press Search+Ctrl+A to view',
      [
        {value: 'name', start: 0, end: 4},
        {value: {delay: true}, start: 5, end: 51},
      ],
      o);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'ContextOrder', async function() {
  this.resetContextualOutput();
  const root =
      await this.runWithLoadedTree('<p>test</p><div role="menu">a</div>');
  let o = new Output().withSpeech(CursorRange.fromNode(root));
  assertEquals('last', o.contextOrder_);

  const p = root.find({role: RoleType.PARAGRAPH});
  const menu = root.find({role: RoleType.MENU});
  o = new Output().withSpeech(
      CursorRange.fromNode(p), CursorRange.fromNode(menu));
  assertEquals('last', o.contextOrder_);

  o = new Output().withSpeech(
      CursorRange.fromNode(menu), CursorRange.fromNode(p));
  assertEquals('first', o.contextOrder_);

  o = new Output().withSpeech(
      CursorRange.fromNode(menu.firstChild), CursorRange.fromNode(p));
  assertEquals('first', o.contextOrder_);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'TreeGridLevel', async function() {
  const site = `
    <table id="treegrid" role="treegrid" aria-label="Inbox">
      <tbody>
        <tr role="row" aria-level="1" aria-posinset="1" aria-setsize="1"
            aria-expanded="true" aria-label="Treegrid faq">
          <td role="gridcell">Treegrids are awesome</td>
          <td role="gridcell">Want to learn how to use them?</td>
        </tr>
      </tbody>
    </table>
  `;
  const root = await this.runWithLoadedTree(site);
  const row = root.find({role: RoleType.ROW});
  let range = CursorRange.fromNode(row);
  let o =
      new Output().withoutHints().withSpeechAndBraille(range, null, 'navigate');

  checkSpeechOutput(
      ' level 1 |Treegrid faq|Expanded|Row',
      [
        {value: 'name', 'start': 10, 'end': 22},
        {value: 'state', start: 23, end: 31},
        {value: 'role', start: 32, end: 35},
      ],
      o);

  range = CursorRange.fromNode(row.firstChild);
  o = new Output().withoutHints().withSpeechAndBraille(range, null, 'navigate');

  checkSpeechOutput(
      'Treegrids are awesome| level 1 |row 1 column 1',
      [{value: 'name', start: 0, end: 21}], o);

  range = CursorRange.fromNode(row.lastChild);
  o = new Output().withoutHints().withSpeechAndBraille(range, null, 'navigate');

  checkSpeechOutput(
      'Want to learn how to use them?|row 1 column 2',
      [{value: 'name', start: 0, end: 30}], o);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'FocusFollowText', async function() {
  const site = `<p>Hello World</p>
                <button>Button</button>
                <div>New Div</div>`;
  const root = await this.runWithLoadedTree(site);
  let called = false;
  let actualBounds = {};

  // Mock call to Accessibility Common extension
  chrome.accessibilityPrivate.setChromeVoxFocus = ((bounds) => {
    called = true;
    actualBounds = bounds;
  });

  // Triggers drawing of the focus ring on text node
  const text = root.find({role: RoleType.STATIC_TEXT});
  new Output().withSpeech(CursorRange.fromNode(text)).go();
  assertTrue(called);
  assertEquals(text.location.left, actualBounds.left);
  assertEquals(text.location.top, actualBounds.top);
  assertEquals(text.location.width, actualBounds.width);
  assertEquals(text.location.height, actualBounds.height);

  // Shift focus to interactive element
  const button = root.find({role: RoleType.BUTTON});
  new Output().withSpeech(CursorRange.fromNode(button)).go();
  assertEquals(button.location.left, actualBounds.left);
  assertEquals(button.location.top, actualBounds.top);
  assertEquals(button.location.width, actualBounds.width);
  assertEquals(button.location.height, actualBounds.height);

  // Ensure focus is shifted to new type of node that contains text
  const div = root.find({role: RoleType.GENERIC_CONTAINER});
  new Output().withSpeech(CursorRange.fromNode(div)).go();
  assertEquals(div.location.left, actualBounds.left);
  assertEquals(div.location.top, actualBounds.top);
  assertEquals(div.location.width, actualBounds.width);
  assertEquals(div.location.height, actualBounds.height);
});

AX_TEST_F('ChromeVoxOutputE2ETest', 'ToStringEmptyOutput', async function() {
  assertEquals('', new Output().toString());
});
