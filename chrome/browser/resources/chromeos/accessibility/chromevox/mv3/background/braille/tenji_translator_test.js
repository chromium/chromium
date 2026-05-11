// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);

/** Returns a braille string built from cell bitmask values. */
function brailleString(...cells) {
  return cells.map(c => String.fromCharCode(BRAILLE_UNICODE_BLOCK_START + c))
      .join('');
}

ChromeVoxTenjiTranslatorTest = class extends ChromeVoxE2ETest {
  async setUpDeferred() {
    await super.setUpDeferred();
    this.translator_ = new TenjiTranslator();
    this.resetState_();
    this.savedInstallTenji_ = chrome.accessibilityPrivate.installTenji;
    this.savedTenjiStartWorker_ = OffscreenBridge.tenjiStartWorker;
    this.savedTenjiTranslate_ = OffscreenBridge.tenjiTranslate;
  }

  tearDown() {
    chrome.accessibilityPrivate.installTenji = this.savedInstallTenji_;
    OffscreenBridge.tenjiStartWorker = this.savedTenjiStartWorker_;
    OffscreenBridge.tenjiTranslate = this.savedTenjiTranslate_;
    this.resetState_();
    super.tearDown();
  }

  resetState_() {
    TenjiTranslator['pendingRequest_'] = false;
    TenjiTranslator['requestQueue_'] = [];
    TenjiTranslator['initPromise_'] = null;
  }

  /** Bypasses initialization and marks the translator as ready. */
  setInitialized_() {
    TenjiTranslator['initPromise_'] = Promise.resolve();
    TenjiTranslator['pendingRequest_'] = false;
  }
};

AX_TEST_F('ChromeVoxTenjiTranslatorTest', 'TranslateBasic', async function() {
  this.setInitialized_();
  OffscreenBridge.tenjiTranslate = (_text) => Promise.resolve({
    value: brailleString(1, 3),
    textToBraille: [0, 1],
    brailleToText: [0, 1],
  });

  await new Promise((resolve) => {
    this.translator_.translate(
        'ab', [], (cells, textToBraille, brailleToText) => {
          const bytes = new Uint8Array(cells);
          assertEquals(2, bytes.length);
          assertEquals(1, bytes[0]);
          assertEquals(3, bytes[1]);
          assertEqualsJSON([0, 1], textToBraille);
          assertEqualsJSON([0, 1], brailleToText);
          resolve();
        });
  });
});

// Newlines (\n) are passed through by the Tenji library and must map to 0
// (empty braille cell) rather than producing a negative byte value.
AX_TEST_F(
    'ChromeVoxTenjiTranslatorTest', 'TranslateLfNewlineMapsToZero',
    async function() {
      this.setInitialized_();
      OffscreenBridge.tenjiTranslate = (_text) => Promise.resolve({
        value: brailleString(5) + '\n' + brailleString(7),
        textToBraille: [0, 1, 2],
        brailleToText: [0, 1, 2],
      });

      await new Promise((resolve) => {
        this.translator_.translate('a\nb', [], (cells, _ttb, _btt) => {
          const bytes = new Uint8Array(cells);
          assertEquals(3, bytes.length);
          assertEquals(5, bytes[0]);
          assertEquals(0, bytes[1]);
          assertEquals(7, bytes[2]);
          resolve();
        });
      });
    });

// Characters above U+28FF are also outside the Braille block and must map to 0
// rather than wrapping silently in Uint8Array.
AX_TEST_F(
    'ChromeVoxTenjiTranslatorTest', 'TranslateCharAboveBrailleBlockMapsToZero',
    async function() {
      this.setInitialized_();
      const aboveBlock = String.fromCharCode(0x2900);  // one past U+28FF
      OffscreenBridge.tenjiTranslate = (_text) => Promise.resolve({
        value: brailleString(1) + aboveBlock + brailleString(3),
        textToBraille: [0, 1, 2],
        brailleToText: [0, 1, 2],
      });

      await new Promise((resolve) => {
        this.translator_.translate('abc', [], (cells, _ttb, _btt) => {
          const bytes = new Uint8Array(cells);
          assertEquals(3, bytes.length);
          assertEquals(1, bytes[0]);
          assertEquals(0, bytes[1]);
          assertEquals(3, bytes[2]);
          resolve();
        });
      });
    });

// CRLF sequences (\r\n) are also passed through and both bytes must map to 0.
AX_TEST_F(
    'ChromeVoxTenjiTranslatorTest', 'TranslateCrlfNewlineMapsToZero',
    async function() {
      this.setInitialized_();
      OffscreenBridge.tenjiTranslate = (_text) => Promise.resolve({
        value: brailleString(2) + '\r\n' + brailleString(4),
        textToBraille: [0, 1, 2, 3],
        brailleToText: [0, 1, 2, 3],
      });

      await new Promise((resolve) => {
        this.translator_.translate('a\r\nb', [], (cells, _ttb, _btt) => {
          const bytes = new Uint8Array(cells);
          assertEquals(4, bytes.length);
          assertEquals(2, bytes[0]);
          assertEquals(0, bytes[1]);
          assertEquals(0, bytes[2]);
          assertEquals(4, bytes[3]);
          resolve();
        });
      });
    });

AX_TEST_F(
    'ChromeVoxTenjiTranslatorTest', 'TranslateNullResultReturnsEmpty',
    async function() {
      this.setInitialized_();
      OffscreenBridge.tenjiTranslate = (_text) => Promise.resolve(null);

      await new Promise((resolve) => {
        this.translator_.translate(
            'ab', [], (cells, textToBraille, brailleToText) => {
              assertEquals(0, cells.byteLength);
              assertEqualsJSON([], textToBraille);
              assertEqualsJSON([], brailleToText);
              resolve();
            });
      });
    });

AX_TEST_F(
    'ChromeVoxTenjiTranslatorTest', 'TranslateRejectionReturnsEmpty',
    async function() {
      this.setInitialized_();
      OffscreenBridge.tenjiTranslate = (_text) =>
          Promise.reject(new Error('translation failed'));

      await new Promise((resolve) => {
        this.translator_.translate(
            'ab', [], (cells, textToBraille, brailleToText) => {
              assertEquals(0, cells.byteLength);
              assertEqualsJSON([], textToBraille);
              assertEqualsJSON([], brailleToText);
              resolve();
            });
      });
    });

// TODO(crbug.com/510816368): Back translation is disabled until full IME
// support is available. Verify the stub immediately returns null.
AX_TEST_F(
    'ChromeVoxTenjiTranslatorTest', 'BackTranslateReturnsNull',
    async function() {
      this.setInitialized_();

      await new Promise((resolve) => {
        this.translator_.backTranslate(
            new Uint8Array([1, 3]).buffer, (text) => {
              assertEquals(null, text);
              resolve();
            });
      });
    });

AX_TEST_F(
    'ChromeVoxTenjiTranslatorTest', 'RequestsQueueWhileNotInitialized',
    async function() {
      OffscreenBridge.tenjiTranslate = (_text) => Promise.resolve({
        value: brailleString(1),
        textToBraille: [0],
        brailleToText: [0],
      });

      let callbackCount = 0;
      const p1 = new Promise((resolve) => {
        this.translator_.translate('a', [], () => {
          callbackCount++;
          resolve();
        });
      });
      const p2 = new Promise((resolve) => {
        this.translator_.translate('b', [], () => {
          callbackCount++;
          resolve();
        });
      });

      assertEquals(0, callbackCount);
      assertEquals(2, TenjiTranslator['requestQueue_'].length);

      TenjiTranslator['initPromise_'] = Promise.resolve();
      void TenjiTranslator['processNextRequest_']();

      await Promise.all([p1, p2]);
      assertEquals(2, callbackCount);
    });

AX_TEST_F('ChromeVoxTenjiTranslatorTest', 'InitSuccess', async function() {
  OffscreenBridge.tenjiStartWorker = () => Promise.resolve();
  chrome.accessibilityPrivate.installTenji = (callback) => {
    callback({wrapperJs: new ArrayBuffer(8), wasm: new ArrayBuffer(8)});
  };

  assertEquals(null, TenjiTranslator['initPromise_']);
  this.translator_.init();
  await TenjiTranslator['initPromise_'];
  assertNotEquals(null, TenjiTranslator['initPromise_']);
});

AX_TEST_F(
    'ChromeVoxTenjiTranslatorTest', 'InitFailureFailsQueuedRequests',
    async function() {
      OffscreenBridge.tenjiStartWorker = () =>
          Promise.reject(new Error('start failed'));
      chrome.accessibilityPrivate.installTenji = (callback) => {
        callback({wrapperJs: new ArrayBuffer(8), wasm: new ArrayBuffer(8)});
      };

      let callbackCount = 0;
      const p1 = new Promise((resolve) => {
        this.translator_.translate('a', [], (cells) => {
          assertEquals(0, cells.byteLength);
          callbackCount++;
          resolve();
        });
      });

      this.translator_.init();
      // initPromise_ resolves (not rejects) once the failure is handled and
      // failQueuedRequests_() has been called.
      await TenjiTranslator['initPromise_'];
      await p1;
      assertEquals(1, callbackCount);
    });
