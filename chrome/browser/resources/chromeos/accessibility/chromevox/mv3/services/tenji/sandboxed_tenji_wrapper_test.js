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
