// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);

/**
 * Test fixture.
 */
ChromeVoxExpandingBrailleTranslatorUnitTest = class extends ChromeVoxE2ETest {};

/**
 * An implementation of {@link LibLouis.Translator} whose translation
 * output is an array buffer of the same byte length as the input and where
 * each byte is equal to the character code of {@code resultChar}.  The
 * position mappings are one to one in both directions.
 */
class FakeTranslator {
  /**
   * @param {string} resultChar A one character string used for each byte of the
   *     result.
   */
  constructor(resultChar, opt_styleMap) {
    /** @private {string} */
    this.resultChar_ = resultChar;
    /** @private {!Object} */
    this.styleMap_ = opt_styleMap || {};
  }

  /** @Override */
  translate(text, formTypeMap, callback) {
    const result = new Uint8Array(text.length);
    const textToBraille = [];
    const brailleToText = [];
    for (let i = 0; i < text.length; ++i) {
      let formType = this.styleMap_[formTypeMap[i]];
      if (formType) {
        formType = formType.charCodeAt(0);
      }
      result[i] = formType || this.resultChar_.charCodeAt(0);
      textToBraille.push(i);
      brailleToText.push(i);
    }
    callback(result.buffer, textToBraille, brailleToText);
  }
}


/**
 * Asserts that a array buffer, viewed as an uint8 array, matches
 * the contents of a string.  The character code of each character of the
 * string shall match the corresponding byte in the array buffer.
 * @param {ArrayBuffer} actual Actual array buffer.
 * @param {string} expected Array of expected bytes.
 */
function assertArrayBufferMatches(expected, actual) {
  assertTrue(actual instanceof ArrayBuffer);
  const a = new Uint8Array(actual);
  assertEquals(expected.length, a.length);
  for (let i = 0; i < a.length; ++i) {
    assertEquals(expected.charCodeAt(i), a[i], 'Position ' + i);
  }
}

AX_TEST_F(
    'ChromeVoxExpandingBrailleTranslatorUnitTest', 'TranslationError',
    function() {
      const text = new Spannable('error ok', new ValueSpan());
      text.setSpan(new ValueSelectionSpan(), 0, 0);
      const contractedTranslator = new FakeTranslator('c');
      // Translator that always results in an error.
      const uncontractedTranslator = {
        translate(text, formTypeMap, callback) {
          callback(null, null, null);
        },
      };
      const translationResult = null;

      const expandingTranslator = new ExpandingBrailleTranslator(
          contractedTranslator, uncontractedTranslator);
      expandingTranslator.translate(
          text, ExpandingBrailleTranslator.ExpansionType.SELECTION,
          function(cells, textToBraille, brailleToText) {
            // Expect the string ' ok' to be translated using the contracted
            // translator.  The preceding part isn't included because it
            // resulted in a translation error.
            assertArrayBufferMatches('ccc', cells);
            assertEqualsJSON([0, 0, 0, 0, 0, 0, 1, 2], textToBraille);
            assertEqualsJSON([5, 6, 7], brailleToText);
          });
    });

// Test for many variations of successful translations.

let totalRunTranslationTests = 0;

/**
 * Performs the translation and checks the output.
 * @param {string} name Name that describes the input for error messages.
 * @param {boolean} contracted Whether to use a contracted translator
 *     in addition to the uncontracted one.
 * @param {ExpandingBrailleTranslator.ExpansionType} valueExpansion
 *     Value expansion argument to pass to the translator.
 * @param {string} text Input string.
 * @param {string} expectedOutput Expected output as a string (see
 *     {@code TESTDATA} below for a description of the format).
 */
function doTranslationTest(
    name, contracted, valueExpansion, text, expectedOutput) {
  try {
    totalRunTranslationTests++;
    const uncontractedTranslator = new FakeTranslator('u');
    let expandingTranslator;
    if (contracted) {
      const contractedTranslator = new FakeTranslator('c');
      expandingTranslator = new ExpandingBrailleTranslator(
          contractedTranslator, uncontractedTranslator);
    } else {
      expandingTranslator =
          new ExpandingBrailleTranslator(uncontractedTranslator);
    }
    const extraCellsSpan = text.getSpanInstanceOf(ExtraCellsSpan);
    let extraCellsSpanPos;
    if (extraCellsSpan) {
      extraCellsSpanPos = text.getSpanStart(extraCellsSpan);
    }
    const expectedTextToBraille = [];
    const expectedBrailleToText = [];
    for (let i = 0, pos = 0; i < text.length; ++i, ++pos) {
      if (i === extraCellsSpanPos) {
        ++pos;
      }
      expectedTextToBraille.push(pos);
      expectedBrailleToText.push(i);
    }
    if (extraCellsSpan) {
      expectedBrailleToText.splice(extraCellsSpanPos, 0, extraCellsSpanPos);
    }

    expandingTranslator.translate(
        text, valueExpansion, function(cells, textToBraille, brailleToText) {
          assertArrayBufferMatches(expectedOutput, cells, name);
          assertEqualsJSON(expectedTextToBraille, textToBraille, name);
          assertEqualsJSON(expectedBrailleToText, brailleToText, name);
        });
  } catch (e) {
    console.error('Subtest ' + name + ' failed.');
    throw e;
  }
}

/**
 * Runs two tests, one with the given values and one with the given values
 * where the text is surrounded by a typical name and role.
 * @param {{name: string, input: string, contractedOutput: string}}
 *     testCase An entry of {@code TESTDATA}.
 * @param {boolean} contracted Whether to use both uncontracted
 *     and contracted translators.
 * @param {ExpandingBrailleTranslation.ExpansionType} valueExpansion
 *     What kind of value expansion to apply.
 * @param {boolean} withExtraCells Whether to insert an extra cells span
 *     right before the selection in the input.
 */
function runTranslationTestVariants(
    testCase, contracted, valueExpansion, withExtraCells) {
  const expType = ExpandingBrailleTranslator.ExpansionType;
  // Construct the full name.
  let fullName = contracted ? 'Contracted_' : 'Uncontracted_';
  fullName += 'Expansion' + valueExpansion + '_';
  if (withExtraCells) {
    fullName += 'ExtraCells_';
  }
  fullName += testCase.name;
  let input = testCase.input;
  let selectionStart;
  if (withExtraCells) {
    input = input.substring(0);  // Shallow copy.
    selectionStart =
        input.getSpanStart(input.getSpanInstanceOf(ValueSelectionSpan));
    const extraCellsSpan = new ExtraCellsSpan();
    extraCellsSpan.cells = new Uint8Array(['e'.charCodeAt(0)]).buffer;
    input.setSpan(extraCellsSpan, selectionStart, selectionStart);
  }
  // The expected output depends on the contraction mode and value expansion.
  const outputChar = contracted ? 'c' : 'u';
  let expectedOutput;
  if (contracted && valueExpansion === expType.SELECTION) {
    expectedOutput = testCase.contractedOutput;
  } else if (contracted && valueExpansion === expType.ALL) {
    expectedOutput = new Array(testCase.input.length + 1).join('u');
  } else {
    expectedOutput = new Array(testCase.input.length + 1).join(outputChar);
  }
  if (withExtraCells) {
    expectedOutput = expectedOutput.substring(0, selectionStart) + 'e' +
        expectedOutput.substring(selectionStart);
  }
  doTranslationTest(
      fullName, contracted, valueExpansion, input, expectedOutput);

  // Run another test, with the value surrounded by some text.
  const surroundedText = new Spannable('Name: ');
  let surroundedExpectedOutput =
      new Array('Name: '.length + 1).join(outputChar);
  surroundedText.append(input);
  surroundedExpectedOutput += expectedOutput;
  if (testCase.input.length > 0) {
    surroundedText.append(' ');
    surroundedExpectedOutput += outputChar;
  }
  surroundedText.append('edtxt');
  surroundedExpectedOutput += new Array('edtxt'.length + 1).join(outputChar);
  doTranslationTest(
      fullName + '_Surrounded', contracted, valueExpansion, surroundedText,
      surroundedExpectedOutput);
}

/**
 * Creates a spannable text with optional selection.
 * @param {string} text The text.
 * @param {=opt_selectionStart} Selection start or caret position.  No
 *     selection is added if undefined.
 * @param {=opt_selectionEnd} Selection end if selection is not a caret.
 */
function createText(text, opt_selectionStart, opt_selectionEnd, opt_style) {
  const result = new Spannable(text);

  result.setSpan(new ValueSpan(), 0, text.length);
  if (opt_selectionStart !== undefined) {
    result.setSpan(
        new ValueSelectionSpan(), opt_selectionStart,
        (opt_selectionEnd !== undefined) ? opt_selectionEnd :
                                           opt_selectionStart);
  }

  if (opt_style !== undefined) {
    result.setSpan(
        new BrailleTextStyleSpan(opt_style.formType), opt_style.start,
        opt_style.end);
  }
  return result;
}


const TEXT = 'Hello, world!';

AX_TEST_F(
    'ChromeVoxExpandingBrailleTranslatorUnitTest', 'successfulTranslations',
    function() {
      /**
       * Dictionary of test strings, keyed on a descriptive name for the
       * test case.  The value is an array of the input string to the
       * translation and the expected output using a translator with both
       * uncontracted and contracted underlying translators.  The expected
       * output is in the form of a string of the same length as the input,
       * where an 'u' means that the uncontracted translator was used at this
       * location and a 'c' means that the contracted translator was used.
       */
      const TESTDATA = [
        {name: 'emptyText', input: createText(''), contractedOutput: ''},
        {
          name: 'emptyTextWithCaret',
          input: createText('', 0),
          contractedOutput: '',
        },
        {
          name: 'textWithNoSelection',
          input: createText(TEXT),
          contractedOutput: 'ccccccccccccc',
        },
        {
          name: 'textWithCaretAtStart',
          input: createText(TEXT, 0),
          contractedOutput: 'uuuuuuccccccc',
        },
        {
          name: 'textWithCaretAtEnd',
          input: createText(TEXT, TEXT.length),
          contractedOutput: 'cccccccuuuuuu',
        },
        {
          name: 'textWithCaretInWhitespace',
          input: createText(TEXT, 6),
          contractedOutput: 'uuuuuuucccccc',
        },
        {
          name: 'textWithSelectionEndInWhitespace',
          input: createText(TEXT, 0, 7),
          contractedOutput: 'uuuuuuucccccc',
        },
        {
          name: 'textWithSelectionInTwoWords',
          input: createText(TEXT, 2, 9),
          contractedOutput: 'uuuuuucuuuuuu',
        },
      ];
      const TESTDATA_WITH_SELECTION = TESTDATA.filter(
          testCase => testCase.input.getSpanInstanceOf(ValueSelectionSpan));

      const expType = ExpandingBrailleTranslator.ExpansionType;
      for (let i = 0, testCase; testCase = TESTDATA[i]; ++i) {
        runTranslationTestVariants(testCase, false, expType.SELECTION, false);
        runTranslationTestVariants(testCase, true, expType.NONE, false);
        runTranslationTestVariants(testCase, true, expType.SELECTION, false);
        runTranslationTestVariants(testCase, true, expType.ALL, false);
      }
      for (let i = 0, testCase; testCase = TESTDATA_WITH_SELECTION[i]; ++i) {
        runTranslationTestVariants(testCase, true, expType.SELECTION, true);
      }

      // Make sure that the logic above runs the tests, adjust when adding more
      // test variants.
      const totalExpectedTranslationTests =
          2 * (TESTDATA.length * 4 + TESTDATA_WITH_SELECTION.length);
      assertEquals(totalExpectedTranslationTests, totalRunTranslationTests);
    });

AX_TEST_F(
    'ChromeVoxExpandingBrailleTranslatorUnitTest', 'StyleTranslations',
    function() {
      const formTypeMap = {};
      formTypeMap[LibLouis.FormType.BOLD] = 'b';
      formTypeMap[LibLouis.FormType.ITALIC] = 'i';
      formTypeMap[LibLouis.FormType.UNDERLINE] = 'u';
      const translator = new ExpandingBrailleTranslator(
          new FakeTranslator('c', formTypeMap), new FakeTranslator('u'));
      translator.translate(
          createText(
              'a test of text', undefined, undefined,
              {start: 2, end: 6, formType: LibLouis.FormType.BOLD}),
          ExpandingBrailleTranslator.ExpansionType.NONE, function(cells) {
            assertArrayBufferMatches('ccbbbbcccccccc', cells);
          });
    });
