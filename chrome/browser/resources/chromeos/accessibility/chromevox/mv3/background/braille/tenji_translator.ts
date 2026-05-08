// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ArrayBufferUtil} from '/common/array_buffer_util.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {OffscreenBridge} from '../../common/offscreen_bridge.js';

import {BackTranslateCallback, BrailleTranslator, TranslateCallback} from './braille_translator.js';

type QueuedRequest = {
  type: 'translate',
  text: string,
  callback: TranslateCallback
}|{type: 'backTranslate', cells: ArrayBuffer, callback: BackTranslateCallback};

export class TenjiTranslator implements BrailleTranslator {
  private static initPromise_: Promise<void>|null = null;
  private static pendingRequest_ = false;
  private static requestQueue_: QueuedRequest[] = [];

  init(): Promise<void> {
    if (!TenjiTranslator.initPromise_) {
      TenjiTranslator.initPromise_ = TenjiTranslator.doInit_();
    }

    return Promise.resolve();
  }

  private static async doInit_(): Promise<void> {
    TenjiTranslator.pendingRequest_ = true;
    try {
      await TenjiTranslator.installTenji_();
      void TenjiTranslator.processNextRequest_();
    } catch (error) {
      console.error('Error during tenji initialization: ' + error);
      TenjiTranslator.initPromise_ = null;
      TenjiTranslator.failQueuedRequests_();
    } finally {
      TenjiTranslator.pendingRequest_ = false;
    }
  }

  private static installTenji_(): Promise<void> {
    return new Promise<void>((resolve, reject) => {
      chrome.accessibilityPrivate.installTenji(
          (data: chrome.accessibilityPrivate.TenjiData) => {
            void TenjiTranslator.onTenjiInstalled_(data).then(resolve).catch(
                reject);
          });
    });
  }

  private static async onTenjiInstalled_(
      data: chrome.accessibilityPrivate.TenjiData): Promise<void> {
    if (!data) {
      console.error('Tenji installation failed');
    }

    await TenjiTranslator.startWorker_(data);
  }

  private static async startWorker_(
      data: chrome.accessibilityPrivate.TenjiData): Promise<void> {
    const tenjiData = {
      wrapperJs: await ArrayBufferUtil.arrayBufferToBase64(data.wrapperJs),
      wasm: await ArrayBufferUtil.arrayBufferToBase64(data.wasm),
    };
    await OffscreenBridge.tenjiStartWorker(tenjiData);
  }

  translate(
      text: string,
      // Tenji library doesn't support form type maps, so this parameter is
      // unused
      _formTypeMap: number[]|number, callback: TranslateCallback): void {
    // Tenji library has a limit defined in terms of UTF-8 bytes on the input
    // text size, so if it exceeds the limit truncate the text without splitting
    // multi-byte characters.
    const truncatedText =
        truncateToMaxUtf8Bytes(text, MAX_TRANSLATE_TEXT_BYTES);
    TenjiTranslator.requestQueue_.push(
        {type: 'translate', text: truncatedText, callback});
    if (!TenjiTranslator.pendingRequest_) {
      void TenjiTranslator.processNextRequest_();
    }
  }

  backTranslate(cells: ArrayBuffer, callback: BackTranslateCallback): void {
    TenjiTranslator.requestQueue_.push(
        {type: 'backTranslate', cells, callback});
    if (!TenjiTranslator.pendingRequest_) {
      void TenjiTranslator.processNextRequest_();
    }
  }

  private static async processNextRequest_(): Promise<void> {
    if (TenjiTranslator.pendingRequest_) {
      return;
    }

    if (!TenjiTranslator.initPromise_) {
      return;
    }

    const req = TenjiTranslator.requestQueue_.shift();
    if (!req) {
      return;
    }

    TenjiTranslator.pendingRequest_ = true;

    try {
      if (req.type === 'translate') {
        const result = await OffscreenBridge.tenjiTranslate(req.text);
        if (!result || !result.value) {
          req.callback(new ArrayBuffer(0), [], []);
        } else {
          const tenjiString = result.value;
          const bytes = new Uint8Array(tenjiString.length);
          for (let i = 0; i < tenjiString.length; i++) {
            // Get the character code and subtract the Braille base to get
            // the cell value, which is expected to be just the bitmask.
            // See `BrailleCaptionsBackground.setContent()` and [1] for
            // more details.
            // The Tenji library passes newlines through unmodified which
            // could result in characters outside the Braille Unicode block.
            // Map any character outside the Braille Unicode block
            // (U+2800–U+28FF) to 0 (empty cell).
            // [1] https://www.unicode.org/charts/PDF/U2800.pdf
            const offset =
                tenjiString.charCodeAt(i) - BRAILLE_UNICODE_BLOCK_START;
            bytes[i] = (offset >= 0 && offset <= 0xFF) ? offset : 0;
          }
          req.callback(
              bytes.buffer, result.textToBraille, result.brailleToText);
        }
      } else {
        const cellBytes = new Uint8Array(req.cells);
        const tenjiChars: string[] = [];
        for (let i = 0; i < cellBytes.length; i++) {
          tenjiChars.push(
              String.fromCharCode(cellBytes[i] + BRAILLE_UNICODE_BLOCK_START));
        }
        const tenjiString = tenjiChars.join('');
        const result = await OffscreenBridge.tenjiBackTranslate(tenjiString);
        req.callback(result);
      }
    } catch (error) {
      if (req.type === 'translate') {
        console.error('Error during tenji translation: ' + error);
        req.callback(new ArrayBuffer(0), [], []);
      } else {
        console.error('Error during tenji back translation: ' + error);
        req.callback(null);
      }
    }

    TenjiTranslator.pendingRequest_ = false;
    void TenjiTranslator.processNextRequest_();
  }

  private static failQueuedRequests_(): void {
    while (TenjiTranslator.requestQueue_.length > 0) {
      const req = TenjiTranslator.requestQueue_.shift();
      if (!req) {
        return;
      }

      if (req.type === 'translate') {
        req.callback(new ArrayBuffer(0), [], []);
      } else {
        req.callback(null);
      }
    }
  }
}

function truncateToMaxUtf8Bytes(text: string, maxBytes: number): string {
  const encoded = UTF8_ENCODER.encode(text);
  if (encoded.length <= maxBytes) {
    return text;
  }

  // Find the last valid UTF-8 character boundary at or before maxBytes.
  // Step back past any continuation bytes (0x80–0xBF), which cannot start a
  // character.
  let end = maxBytes;
  while (end > 0 && (encoded[end] & 0xC0) === 0x80) {
    end--;
  }
  return UTF8_DECODER.decode(encoded.subarray(0, end));
}

const MAX_TRANSLATE_TEXT_BYTES = 256 * 1024;
const UTF8_ENCODER = new TextEncoder();
const UTF8_DECODER = new TextDecoder();
const BRAILLE_UNICODE_BLOCK_START = 0x2800;

TestImportManager.exportForTesting(TenjiTranslator);
