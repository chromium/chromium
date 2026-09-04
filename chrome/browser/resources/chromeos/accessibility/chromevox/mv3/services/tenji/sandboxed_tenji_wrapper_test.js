// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);

const BRAILLE_BASE = 0x2800;

/** Returns a braille string built from cell bitmask values. */
function brailleString(...cells) {
  return cells.map(c => String.fromCharCode(BRAILLE_BASE + c)).join('');
}

/**
 * Creates a mock TenjiModule. ToTenjiWithOffsetMap returns brailleResult;
 * OffsetMap.MapForward delegates to mapForward.
 */
function makeMockModule(brailleResult, mapForward) {
  const MockOffsetMap = class {
    Copy(_bytes) {}
    MapForward(byte) {
      return mapForward(byte);
    }
    delete() {}
  };
  return {
    ToTenjiWithOffsetMap: (_text, _map) =>
        ({ok: true, value: brailleResult, delete () {}}),
    TenjiToHiragana: (_tenji) =>
        ({ok: true, value: 'mock_hiragana', delete () {}}),
    OffsetMap: MockOffsetMap,
    MozcConvertHiraganaToKanji: (_reading, _maxCandidates) =>
        makeMockEmbindStringVector([]),
  };
}

/**
 * Creates a mock Embind std::vector<std::string>, as returned by
 * MozcConvertHiraganaToKanji, that records whether delete() was called.
 */
function makeMockEmbindStringVector(values, onDelete) {
  return {
    size: () => values.length,
    get: (index) => values[index],
    delete: () => {
      if (onDelete) {
        onDelete();
      }
    },
  };
}

ChromeVoxSandboxedTenjiWrapperTest = class extends ChromeVoxE2ETest {
  tearDown() {
    setTenjiModuleForTesting(null);
    setPostToParentForTesting(null);
    super.tearDown();
  }
};

// --- Tests for translate requests ---

// No module loaded: expect an error response.
AX_TEST_F(
    'ChromeVoxSandboxedTenjiWrapperTest', 'TranslateNotInitialized',
    function() {
      let response = null;
      setPostToParentForTesting((msg) => {
        response = msg;
      });
      handleSandboxMessageForTesting({data: {type: 'translate', text: 'hi'}});
      assertEquals('translate', response.type);
      assertTrue(!!response.error);
      assertEquals(undefined, response.value);
    });

// ASCII input, one text char → one braille cell.
// 'あい' → '⠁⠃': input byte 0 → output byte 0, input byte 1 → output byte 3.
AX_TEST_F(
    'ChromeVoxSandboxedTenjiWrapperTest', 'TranslateAsciiOneToOne', function() {
      const braille = brailleString(1, 3);
      setTenjiModuleForTesting(
          makeMockModule(braille, (byte) => byte === 0 ? 0 : 3));
      let response = null;
      setPostToParentForTesting((msg) => {
        response = msg;
      });
      handleSandboxMessageForTesting({data: {type: 'translate', text: 'あい'}});
      assertEquals('translate', response.type);
      assertEquals(braille, response.value);
      assertEqualsJSON([0, 1], response.textToBraille);
      assertEqualsJSON([0, 1], response.brailleToText);
    });

// One text char maps to two braille cells.
// 'a' → '⠰⠁': all input bytes map to output byte 0.
AX_TEST_F(
    'ChromeVoxSandboxedTenjiWrapperTest', 'TranslateOneTextToTwoCells',
    function() {
      const braille = brailleString(1, 3);
      setTenjiModuleForTesting(makeMockModule(braille, (_byte) => 0));
      let response = null;
      setPostToParentForTesting((msg) => {
        response = msg;
      });
      handleSandboxMessageForTesting({data: {type: 'translate', text: 'a'}});
      assertEquals('translate', response.type);
      assertEqualsJSON([0], response.textToBraille);
      assertEqualsJSON([0, 0], response.brailleToText);
    });

// Two text chars map to one braille cell.
// brailleToText inverts textToBraille monotonically, so the single cell points
// to the last text char that mapped into it.
AX_TEST_F(
    'ChromeVoxSandboxedTenjiWrapperTest', 'TranslateTwoCharsToOneCell',
    function() {
      const braille = brailleString(1);
      setTenjiModuleForTesting(makeMockModule(braille, (_byte) => 0));
      let response = null;
      setPostToParentForTesting((msg) => {
        response = msg;
      });
      handleSandboxMessageForTesting({data: {type: 'translate', text: 'ab'}});
      assertEquals('translate', response.type);
      assertEqualsJSON([0, 0], response.textToBraille);
      assertEqualsJSON([1], response.brailleToText);
    });

// Hiragana input (3-byte UTF-8), one char per braille cell.
// 'あ' starts at input byte 0, 'い' at byte 3; same for output.
AX_TEST_F(
    'ChromeVoxSandboxedTenjiWrapperTest', 'TranslateHiraganaOneToOne',
    function() {
      const braille = brailleString(5, 7);
      setTenjiModuleForTesting(
          makeMockModule(braille, (byte) => byte < 3 ? 0 : 3));
      let response = null;
      setPostToParentForTesting((msg) => {
        response = msg;
      });
      handleSandboxMessageForTesting({data: {type: 'translate', text: 'あい'}});
      assertEquals('translate', response.type);
      assertEqualsJSON([0, 1], response.textToBraille);
      assertEqualsJSON([0, 1], response.brailleToText);
    });

// Empty braille result: textToBraille clamps to 0 for all input chars.
AX_TEST_F(
    'ChromeVoxSandboxedTenjiWrapperTest', 'TranslateEmptyBrailleResult',
    function() {
      setTenjiModuleForTesting(makeMockModule('', (_byte) => 0));
      let response = null;
      setPostToParentForTesting((msg) => {
        response = msg;
      });
      handleSandboxMessageForTesting({data: {type: 'translate', text: 'ab'}});
      assertEquals('translate', response.type);
      assertEqualsJSON([0, 0], response.textToBraille);
      assertEqualsJSON([], response.brailleToText);
    });

// result.ok === false: handler should post an error and still call delete().
AX_TEST_F(
    'ChromeVoxSandboxedTenjiWrapperTest', 'TranslateResultOkFalse', function() {
      let deleteCalledOnResult = false;
      const mockModule = makeMockModule('', (_byte) => 0);
      mockModule.ToTenjiWithOffsetMap = (_text, _map) => ({
        ok: false,
        value: '',
        delete () {
          deleteCalledOnResult = true;
        },
      });
      setTenjiModuleForTesting(mockModule);
      let response = null;
      setPostToParentForTesting((msg) => {
        response = msg;
      });
      handleSandboxMessageForTesting({data: {type: 'translate', text: 'あ'}});
      assertEquals('translate', response.type);
      assertTrue(!!response.error);
      assertEquals(undefined, response.value);
      assertTrue(deleteCalledOnResult);
    });

// result.delete() must be called after a successful translate.
AX_TEST_F(
    'ChromeVoxSandboxedTenjiWrapperTest', 'TranslateDeleteCalledOnResult',
    function() {
      let deleteCalledOnResult = false;
      const mockModule = makeMockModule(brailleString(1), (_byte) => 0);
      mockModule.ToTenjiWithOffsetMap = (_text, _map) => ({
        ok: true,
        value: brailleString(1),
        delete () {
          deleteCalledOnResult = true;
        },
      });
      setTenjiModuleForTesting(mockModule);
      let response = null;
      setPostToParentForTesting((msg) => {
        response = msg;
      });
      handleSandboxMessageForTesting({data: {type: 'translate', text: 'a'}});
      assertEquals('translate', response.type);
      assertEquals(undefined, response.error);
      assertTrue(deleteCalledOnResult);
    });

// --- Tests for backTranslate requests ---

// No module loaded: expect an error response.
AX_TEST_F(
    'ChromeVoxSandboxedTenjiWrapperTest', 'BackTranslateNotInitialized',
    function() {
      let response = null;
      setPostToParentForTesting((msg) => {
        response = msg;
      });
      handleSandboxMessageForTesting(
          {data: {type: 'backTranslate', tenjiString: brailleString(1, 2)}});
      assertEquals('backTranslate', response.type);
      assertTrue(!!response.error);
      assertEquals(undefined, response.value);
    });

AX_TEST_F(
    'ChromeVoxSandboxedTenjiWrapperTest', 'BackTranslateSuccess', function() {
      const mockModule = makeMockModule('', (_byte) => 0);
      mockModule.TenjiToHiragana = (_tenji) =>
          ({ok: true, value: 'あいう', delete () {}});
      setTenjiModuleForTesting(mockModule);
      let response = null;
      setPostToParentForTesting((msg) => {
        response = msg;
      });
      handleSandboxMessageForTesting(
          {data: {type: 'backTranslate', tenjiString: brailleString(1, 2, 3)}});
      assertEquals('backTranslate', response.type);
      assertEquals('あいう', response.value);
      assertEquals(undefined, response.error);
    });

// result.ok === false: handler should post an error and still call delete().
AX_TEST_F(
    'ChromeVoxSandboxedTenjiWrapperTest', 'BackTranslateResultOkFalse',
    function() {
      let deleteCalledOnResult = false;
      const mockModule = makeMockModule('', (_byte) => 0);
      mockModule.TenjiToHiragana = (_tenji) => ({
        ok: false,
        value: '',
        delete () {
          deleteCalledOnResult = true;
        },
      });
      setTenjiModuleForTesting(mockModule);
      let response = null;
      setPostToParentForTesting((msg) => {
        response = msg;
      });
      handleSandboxMessageForTesting(
          {data: {type: 'backTranslate', tenjiString: brailleString(1)}});
      assertEquals('backTranslate', response.type);
      assertTrue(!!response.error);
      assertEquals(undefined, response.value);
      assertTrue(deleteCalledOnResult);
    });

// result.delete() must be called after a successful backTranslate.
AX_TEST_F(
    'ChromeVoxSandboxedTenjiWrapperTest', 'BackTranslateDeleteCalledOnResult',
    function() {
      let deleteCalledOnResult = false;
      const mockModule = makeMockModule('', (_byte) => 0);
      mockModule.TenjiToHiragana = (_tenji) => ({
        ok: true,
        value: 'あ',
        delete () {
          deleteCalledOnResult = true;
        },
      });
      setTenjiModuleForTesting(mockModule);
      let response = null;
      setPostToParentForTesting((msg) => {
        response = msg;
      });
      handleSandboxMessageForTesting(
          {data: {type: 'backTranslate', tenjiString: brailleString(1)}});
      assertEquals('backTranslate', response.type);
      assertEquals(undefined, response.error);
      assertTrue(deleteCalledOnResult);
    });

// --- Tests for convert requests ---

// No module loaded: expect an error response.
AX_TEST_F(
    'ChromeVoxSandboxedTenjiWrapperTest', 'ConvertNotInitialized', function() {
      let response = null;
      setPostToParentForTesting((msg) => {
        response = msg;
      });
      handleSandboxMessageForTesting(
          {data: {type: 'convert', reading: 'てんじ', maxCandidates: 20}});
      assertEquals('convert', response.type);
      assertTrue(!!response.error);
      assertEquals(undefined, response.candidates);
    });

// The reading and maxCandidates are forwarded to the engine, and its
// candidates are unpacked from the Embind vector into a plain array.
AX_TEST_F('ChromeVoxSandboxedTenjiWrapperTest', 'ConvertSuccess', function() {
  const mockModule = makeMockModule('', (_byte) => 0);
  let capturedReading = null;
  let capturedMaxCandidates = null;
  mockModule.MozcConvertHiraganaToKanji = (reading, maxCandidates) => {
    capturedReading = reading;
    capturedMaxCandidates = maxCandidates;
    return makeMockEmbindStringVector(['転じ', '点字', '展示']);
  };
  setTenjiModuleForTesting(mockModule);
  let response = null;
  setPostToParentForTesting((msg) => {
    response = msg;
  });
  handleSandboxMessageForTesting(
      {data: {type: 'convert', reading: 'てんじ', maxCandidates: 20}});
  assertEquals('convert', response.type);
  assertEquals(undefined, response.error);
  assertEqualsJSON(['転じ', '点字', '展示'], response.candidates);
  assertEquals('てんじ', capturedReading);
  assertEquals(20, capturedMaxCandidates);
});

// An empty candidate vector is a valid, successful result (no conversion
// available), distinct from an error.
AX_TEST_F(
    'ChromeVoxSandboxedTenjiWrapperTest', 'ConvertEmptyResult', function() {
      const mockModule = makeMockModule('', (_byte) => 0);
      mockModule.MozcConvertHiraganaToKanji = (_reading, _maxCandidates) =>
          makeMockEmbindStringVector([]);
      setTenjiModuleForTesting(mockModule);
      let response = null;
      setPostToParentForTesting((msg) => {
        response = msg;
      });
      handleSandboxMessageForTesting(
          {data: {type: 'convert', reading: 'てんじ', maxCandidates: 20}});
      assertEquals('convert', response.type);
      assertEquals(undefined, response.error);
      assertEqualsJSON([], response.candidates);
    });

// The engine throwing should be reported as an error response rather than
// propagating, and the (possibly partial) result must still be freed.
AX_TEST_F(
    'ChromeVoxSandboxedTenjiWrapperTest', 'ConvertEngineThrows', function() {
      const mockModule = makeMockModule('', (_byte) => 0);
      mockModule.MozcConvertHiraganaToKanji = (_reading, _maxCandidates) => {
        throw new Error('conversion failed');
      };
      setTenjiModuleForTesting(mockModule);
      let response = null;
      setPostToParentForTesting((msg) => {
        response = msg;
      });
      handleSandboxMessageForTesting(
          {data: {type: 'convert', reading: 'てんじ', maxCandidates: 20}});
      assertEquals('convert', response.type);
      assertTrue(!!response.error);
      assertEquals(undefined, response.candidates);
    });

// result.delete() must be called after a successful convert, to free the
// Embind-bound vector.
AX_TEST_F(
    'ChromeVoxSandboxedTenjiWrapperTest', 'ConvertDeleteCalledOnResult',
    function() {
      let deleteCalledOnResult = false;
      const mockModule = makeMockModule('', (_byte) => 0);
      mockModule.MozcConvertHiraganaToKanji = (_reading, _maxCandidates) =>
          makeMockEmbindStringVector(
              ['転じ'], () => deleteCalledOnResult = true);
      setTenjiModuleForTesting(mockModule);
      setPostToParentForTesting(() => {});
      handleSandboxMessageForTesting(
          {data: {type: 'convert', reading: 'てんじ', maxCandidates: 20}});
      assertTrue(deleteCalledOnResult);
    });
