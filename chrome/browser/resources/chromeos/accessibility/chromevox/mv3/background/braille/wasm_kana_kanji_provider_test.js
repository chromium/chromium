// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);

ChromeVoxWasmKanaKanjiProviderTest = class extends ChromeVoxE2ETest {
  async setUpDeferred() {
    await super.setUpDeferred();
    this.provider_ = new WasmKanaKanjiProvider();
    this.savedInstallTenji_ = chrome.accessibilityPrivate.installTenji;
    this.savedTenjiStartWorker_ = OffscreenBridge.tenjiStartWorker;
    this.savedTenjiConvert_ = OffscreenBridge.tenjiConvert;
    this.resetState_();
  }

  tearDown() {
    chrome.accessibilityPrivate.installTenji = this.savedInstallTenji_;
    OffscreenBridge.tenjiStartWorker = this.savedTenjiStartWorker_;
    OffscreenBridge.tenjiConvert = this.savedTenjiConvert_;
    this.resetState_();
    super.tearDown();
  }

  resetState_() {
    TenjiTranslator['pendingRequest_'] = false;
    TenjiTranslator['requestQueue_'] = [];
    TenjiTranslator['initPromise_'] = null;
  }

  /** Bypasses the DLC install/worker-start path and marks init as ready. */
  setInitSucceeds_() {
    OffscreenBridge.tenjiStartWorker = () => Promise.resolve();
    chrome.accessibilityPrivate.installTenji = (callback) => {
      callback({wrapperJs: new ArrayBuffer(8), wasm: new ArrayBuffer(8)});
    };
  }
};

// The reading, and maxCandidates, reach OffscreenBridge.tenjiConvert
// unchanged, and its resolved candidates are returned as-is.
AX_TEST_F(
    'ChromeVoxWasmKanaKanjiProviderTest', 'GetCandidatesReturnsEngineResult',
    async function() {
      this.setInitSucceeds_();
      let capturedReading = null;
      let capturedMaxCandidates = null;
      OffscreenBridge.tenjiConvert = (reading, maxCandidates) => {
        capturedReading = reading;
        capturedMaxCandidates = maxCandidates;
        return Promise.resolve(['転じ', '点字', '展示']);
      };

      const candidates = await this.provider_.getCandidates('てんじ');
      assertEqualsJSON(['転じ', '点字', '展示'], candidates);
      assertEquals('てんじ', capturedReading);
      assertEquals(20, capturedMaxCandidates);
    });

// A trailing word-separator (the blank cell that ends composition input) is
// trimmed before the reading is handed to the engine.
AX_TEST_F(
    'ChromeVoxWasmKanaKanjiProviderTest', 'GetCandidatesTrimsTrailingSpace',
    async function() {
      this.setInitSucceeds_();
      let capturedReading = null;
      OffscreenBridge.tenjiConvert = (reading, _maxCandidates) => {
        capturedReading = reading;
        return Promise.resolve(['転じ']);
      };

      await this.provider_.getCandidates('てんじ ');
      assertEquals('てんじ', capturedReading);
    });

// Input with no hiragana (e.g. already-committed Kanji, or an empty reading)
// is not sent to the engine at all; it is not convertible.
AX_TEST_F(
    'ChromeVoxWasmKanaKanjiProviderTest', 'GetCandidatesNoHiraganaReturnsEmpty',
    async function() {
      this.setInitSucceeds_();
      let convertCalled = false;
      OffscreenBridge.tenjiConvert = (_reading, _maxCandidates) => {
        convertCalled = true;
        return Promise.resolve(['unexpected']);
      };

      assertEqualsJSON([], await this.provider_.getCandidates('漢字'));
      assertEqualsJSON([], await this.provider_.getCandidates(''));
      assertFalse(convertCalled);
    });

// If TenjiTranslator initialization fails, no conversion is attempted.
AX_TEST_F(
    'ChromeVoxWasmKanaKanjiProviderTest',
    'GetCandidatesInitFailureReturnsEmpty', async function() {
      OffscreenBridge.tenjiStartWorker = () =>
          Promise.reject(new Error('start failed'));
      chrome.accessibilityPrivate.installTenji = (callback) => {
        callback({wrapperJs: new ArrayBuffer(8), wasm: new ArrayBuffer(8)});
      };
      let convertCalled = false;
      OffscreenBridge.tenjiConvert = (_reading, _maxCandidates) => {
        convertCalled = true;
        return Promise.resolve(['unexpected']);
      };

      assertEqualsJSON([], await this.provider_.getCandidates('てんじ'));
      assertFalse(convertCalled);
    });

// A rejected conversion request (e.g. the sandboxed WASM call throwing) is
// caught and surfaced as no candidates, not a rejected promise.
AX_TEST_F(
    'ChromeVoxWasmKanaKanjiProviderTest',
    'GetCandidatesConvertRejectionReturnsEmpty', async function() {
      this.setInitSucceeds_();
      OffscreenBridge.tenjiConvert = (_reading, _maxCandidates) =>
          Promise.reject(new Error('conversion failed'));

      assertEqualsJSON([], await this.provider_.getCandidates('てんじ'));
    });

AX_TEST_F('ChromeVoxWasmKanaKanjiProviderTest', 'ContainsHiragana', function() {
  assertTrue(containsHiragana('てんじ'));
  assertTrue(containsHiragana('転てんじ'));
  assertFalse(containsHiragana('転字'));
  assertFalse(containsHiragana('テンジ'));
  assertFalse(containsHiragana(''));
});
