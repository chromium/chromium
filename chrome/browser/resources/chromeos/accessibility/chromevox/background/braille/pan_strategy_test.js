// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);

/**
 * Test fixture.
 */
ChromeVoxPanStrategyUnitTest = class extends ChromeVoxE2ETest {};

/**
 * Creates an array buffer based off of the passed in content.
 * Note: Input should be a string of numbers, spaces will turn to 0's.
 * @param {string} content String representing the content.
 */
function createArrayBuffer(content) {
  const result = new ArrayBuffer(content.length);
  const view = new Uint8Array(result);
  for (let i = 0; i < content.length; ++i) {
    view[i] = content[i];
  }
  return result;
}

AX_TEST_F('ChromeVoxPanStrategyUnitTest', 'FixedPanning', function() {
  const panner = new PanStrategy();
  panner.setPanStrategy(false);

  panner.setDisplaySize(0, 0);
  panner.setContent('', createArrayBuffer(''), [], 0);
  assertEqualsJSON({firstRow: 0, lastRow: 0}, panner.viewPort);
  assertFalse(panner.previous());
  assertFalse(panner.next());

  // 25 cells with a blank cell in the first 10 characters.
  const translatedContent = createArrayBuffer('01234567 9012345678901234');
  panner.setContent('unused', translatedContent, [], 0);
  assertEqualsJSON({firstRow: 0, lastRow: 0}, panner.viewPort);
  assertFalse(panner.next());
  assertFalse(panner.previous());

  panner.setDisplaySize(1, 10);
  assertEquals(panner.displaySize.columns, 10);
  assertEqualsJSON({firstRow: 0, lastRow: 0}, panner.viewPort);
  assertTrue(panner.next());
  assertEqualsJSON({firstRow: 1, lastRow: 1}, panner.viewPort);
  assertTrue(panner.next());
  assertEqualsJSON({firstRow: 2, lastRow: 2}, panner.viewPort);
  assertFalse(panner.next());
  assertEqualsJSON({firstRow: 2, lastRow: 2}, panner.viewPort);
  assertTrue(panner.previous());
  assertEqualsJSON({firstRow: 1, lastRow: 1}, panner.viewPort);
  assertTrue(panner.previous());
  assertEqualsJSON({firstRow: 0, lastRow: 0}, panner.viewPort);

  panner.setContent('a', translatedContent, [], 19);
  assertEqualsJSON({firstRow: 1, lastRow: 1}, panner.viewPort);

  panner.setContent('a', translatedContent, [], 20);
  assertEqualsJSON({firstRow: 2, lastRow: 2}, panner.viewPort);

  panner.setDisplaySize(1, 8);
  assertEqualsJSON({firstRow: 0, lastRow: 0}, panner.viewPort);

  // Test Multi-line Panning.
  panner.setDisplaySize(2, 10);
  assertEqualsJSON({firstRow: 0, lastRow: 1}, panner.viewPort);
  assertTrue(panner.next());
  assertEqualsJSON({firstRow: 2, lastRow: 2}, panner.viewPort);
  assertFalse(panner.next());
  assertEqualsJSON({firstRow: 2, lastRow: 2}, panner.viewPort);
  assertTrue(panner.previous());
  assertEqualsJSON({firstRow: 0, lastRow: 1}, panner.viewPort);
  assertFalse(panner.previous());
  assertEqualsJSON({firstRow: 0, lastRow: 1}, panner.viewPort);
});

AX_TEST_F(
    'ChromeVoxPanStrategyUnitTest', 'WrappedPanningSingleLine', function() {
      const panner = new PanStrategy();
      panner.setPanStrategy(true);

      // 30 cells with blank cells at positions 8, 22 and 26.
      const content = createArrayBuffer('11234567 9112345678911 345 789');
      panner.setContent('a', content, [], 0);
      assertEqualsJSON({firstRow: 0, lastRow: 0}, panner.viewPort);
      assertFalse(panner.next());
      assertFalse(panner.previous());

      panner.setDisplaySize(1, 10);
      assertEqualsJSON({firstRow: 0, lastRow: 0}, panner.viewPort);
      assertArrayBuffersEquals(
          createArrayBuffer('11234567  '),
          panner.getCurrentBrailleViewportContents());
      assertTrue(panner.next());
      assertEqualsJSON({firstRow: 1, lastRow: 1}, panner.viewPort);
      assertArrayBuffersEquals(
          createArrayBuffer('9112345678'),
          panner.getCurrentBrailleViewportContents());
      assertTrue(panner.next());
      assertEqualsJSON({firstRow: 2, lastRow: 2}, panner.viewPort);
      assertArrayBuffersEquals(
          createArrayBuffer('911 345   '),
          panner.getCurrentBrailleViewportContents());
      assertTrue(panner.next());
      assertEqualsJSON({firstRow: 3, lastRow: 3}, panner.viewPort);
      assertArrayBuffersEquals(
          createArrayBuffer('789'), panner.getCurrentBrailleViewportContents());
      assertFalse(panner.next());
      assertEqualsJSON({firstRow: 3, lastRow: 3}, panner.viewPort);
      assertTrue(panner.previous());
      assertEqualsJSON({firstRow: 2, lastRow: 2}, panner.viewPort);
      assertTrue(panner.previous());
      assertEqualsJSON({firstRow: 1, lastRow: 1}, panner.viewPort);
      assertTrue(panner.previous());
      assertEqualsJSON({firstRow: 0, lastRow: 0}, panner.viewPort);
      assertFalse(panner.previous());

      panner.setContent('a', content, [], 21);
      assertEqualsJSON({firstRow: 2, lastRow: 2}, panner.viewPort);
      assertArrayBuffersEquals(
          createArrayBuffer('911 345   '),
          panner.getCurrentBrailleViewportContents());

      panner.setContent('a', content, [], 30);
      assertEqualsJSON({firstRow: 3, lastRow: 3}, panner.viewPort);
      assertArrayBuffersEquals(
          createArrayBuffer('789'), panner.getCurrentBrailleViewportContents());

      panner.setDisplaySize(1, 8);
      assertEqualsJSON({firstRow: 0, lastRow: 0}, panner.viewPort);
      assertArrayBuffersEquals(
          createArrayBuffer('11234567'),
          panner.getCurrentBrailleViewportContents());
    });

AX_TEST_F(
    'ChromeVoxPanStrategyUnitTest', 'WrappedPanningMultiline', function() {
      const panner = new PanStrategy();
      panner.setPanStrategy(true);

      // 30 cells with blank cells at positions 8, 22 and 26.
      const content = createArrayBuffer('11234567 9112345678911 345 789');
      panner.setContent('a', content, [], 0);

      panner.setDisplaySize(2, 10);
      assertEqualsJSON({firstRow: 0, lastRow: 1}, panner.viewPort);
      assertArrayBuffersEquals(
          createArrayBuffer('11234567  9112345678'),
          panner.getCurrentBrailleViewportContents());
      assertTrue(panner.next());
      assertEqualsJSON({firstRow: 2, lastRow: 3}, panner.viewPort);
      assertArrayBuffersEquals(
          createArrayBuffer('911 345   789'),
          panner.getCurrentBrailleViewportContents());
      assertFalse(panner.next());
      assertEqualsJSON({firstRow: 2, lastRow: 3}, panner.viewPort);
      assertArrayBuffersEquals(
          createArrayBuffer('911 345   789'),
          panner.getCurrentBrailleViewportContents());
      assertTrue(panner.previous());
      assertEqualsJSON({firstRow: 0, lastRow: 1}, panner.viewPort);
      assertArrayBuffersEquals(
          createArrayBuffer('11234567  9112345678'),
          panner.getCurrentBrailleViewportContents());
      assertFalse(panner.previous());
      assertEqualsJSON({firstRow: 0, lastRow: 1}, panner.viewPort);
      assertArrayBuffersEquals(
          createArrayBuffer('11234567  9112345678'),
          panner.getCurrentBrailleViewportContents());
    });

AX_TEST_F('ChromeVoxPanStrategyUnitTest', 'FixedSetContent', function() {
  const panner = new PanStrategy();
  panner.setPanStrategy(false);

  const textContent = 'ABCDE FGHI';
  const translatedContent = createArrayBuffer('11234 6789');
  const mapping = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
  panner.setDisplaySize(1, 5);
  panner.setContent(textContent, translatedContent, mapping, 0);
  const expectedBufferValue = translatedContent;
  assertArrayBuffersEquals(expectedBufferValue, panner.fixedBuffer_);
  const expectedMappingValue = mapping;
  assertArraysEquals(expectedMappingValue, panner.brailleToText);
});

AX_TEST_F('ChromeVoxPanStrategyUnitTest', 'WrappedSetContent', function() {
  const panner = new PanStrategy();
  panner.setPanStrategy(true);

  // When first word is bigger than column size. (Don't wrap word)
  let textContent = 'ABCDE';
  let translatedContent = createArrayBuffer('11234');
  let mapping = [0, 1, 2, 3, 4];
  panner.setDisplaySize(1, 4);
  panner.setContent(textContent, translatedContent, mapping, 0);
  let expectedBufferValue = translatedContent;
  assertArrayBuffersEquals(expectedBufferValue, panner.wrappedBuffer_);
  let expectedMappingValue = mapping;
  assertArraysEquals(expectedMappingValue, panner.brailleToText);

  // When first word is equal to column size.
  // (We expect space to be removed on next line)
  textContent = 'ABCDE FGHI';
  translatedContent = createArrayBuffer('11234 6789');
  mapping = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
  panner.setDisplaySize(1, 5);
  panner.setContent(textContent, translatedContent, mapping, 0);
  expectedBufferValue = createArrayBuffer('112346789');
  assertArrayBuffersEquals(expectedBufferValue, panner.wrappedBuffer_);
  expectedMappingValue = [0, 1, 2, 3, 4, 6, 7, 8, 9];
  assertArraysEquals(expectedMappingValue, panner.brailleToText);

  // When first word is equal to column size + 1. (Space stays)
  panner.setDisplaySize(1, 6);
  expectedBufferValue = createArrayBuffer('11234 6789');
  assertArrayBuffersEquals(expectedBufferValue, panner.wrappedBuffer_);
  expectedMappingValue = mapping;
  assertArraysEquals(expectedMappingValue, panner.brailleToText);

  // When first word is smaller than column size but second word is too big
  // to fit the same line. (Pad that line, move second word to next line)
  panner.setDisplaySize(1, 7);
  expectedBufferValue = createArrayBuffer('11234  6789');
  assertArrayBuffersEquals(expectedBufferValue, panner.wrappedBuffer_);
  expectedMappingValue = [0, 1, 2, 3, 4, 5, 5, 6, 7, 8, 9];
  assertArraysEquals(expectedMappingValue, panner.brailleToText);

  // Test all excess spaces are removed.
  panner.setDisplaySize(1, 6);
  textContent = 'ABCDEF GHI';
  translatedContent = createArrayBuffer('112345     789');
  mapping = [0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 7, 7, 8, 9, 10];
  panner.setContent(textContent, translatedContent, mapping, 0);
  expectedBufferValue = createArrayBuffer('112345789');
  assertArrayBuffersEquals(expectedBufferValue, panner.wrappedBuffer_);
  expectedMappingValue = [0, 1, 2, 3, 4, 5, 7, 8, 9];
  assertArraysEquals(expectedMappingValue, panner.brailleToText);
});

AX_TEST_F(
    'ChromeVoxPanStrategyUnitTest', 'getCurrentTextViewportContents',
    function() {
      const panner = new PanStrategy();
      panner.setPanStrategy(true);

      // 30 cells with blank cells at positions 8, 22 and 26.
      const content = createArrayBuffer('11234567 9112345678911 345 789');
      const mapping = [
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
      ];
      panner.setContent('11234567 9112345678911 345 789', content, mapping, 0);
      assertEqualsJSON({firstRow: 0, lastRow: 0}, panner.viewPort);
      assertFalse(panner.next());
      assertFalse(panner.previous());

      panner.setDisplaySize(1, 10);
      assertEqualsJSON({firstRow: 0, lastRow: 0}, panner.viewPort);
      assertEquals('11234567 ', panner.getCurrentTextViewportContents());
      panner.next();
      assertEquals('9112345678', panner.getCurrentTextViewportContents());
      panner.next();
      assertEquals('911 345 ', panner.getCurrentTextViewportContents());
      panner.next();
      assertEquals('789', panner.getCurrentTextViewportContents());
    });

AX_TEST_F(
    'ChromeVoxPanStrategyUnitTest', 'WrappedUnwrappedCursors', function() {
      const panner = new PanStrategy();
      panner.setPanStrategy(true);

      // 30 cells with blank cells at positions 8, 22 and 26.
      const content = createArrayBuffer('11234567 9112345678911 345 789');

      panner.setCursor(1, 3);
      panner.setContent('a', content, [], 0);
      panner.setDisplaySize(2, 10);
      assertEqualsJSON({start: 1, end: 3}, panner.getCursor());
      assertEqualsJSON({start: 1, end: 3}, panner.wrappedCursor_);

      panner.setCursor(5, 10);
      panner.setContent('a', content, [], 0);
      assertEqualsJSON({start: 5, end: 10}, panner.getCursor());
      assertEqualsJSON({start: 5, end: 11}, panner.wrappedCursor_);

      panner.setCursor(9, 9);
      panner.setContent('a', content, [], 0);
      assertEqualsJSON({start: 9, end: 9}, panner.getCursor());
      assertEqualsJSON({start: 10, end: 11}, panner.wrappedCursor_);
    });
