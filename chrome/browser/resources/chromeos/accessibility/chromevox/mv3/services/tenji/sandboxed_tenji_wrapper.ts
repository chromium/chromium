// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {StringUtil} from '/common/string_util.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

export {};

interface TenjiData {
  wrapperJs: Uint8Array;
  wasm: ArrayBuffer;
}

interface InitMessage {
  type: 'init';
  tenjiData: TenjiData;
}

interface TranslateMessage {
  type: 'translate';
  text: string;
}

interface TranslateResult {
  ok: boolean;
  value: string;
  delete(): void;
}

interface BackTranslateResult {
  ok: boolean;
  value: string;
  delete(): void;
}

interface OffsetMap {
  Copy(bytes: number): void;
  MapForward(aoffset: number): number;
  MapBack(aprimeoffset: number): number;
  delete(): void;  // Emscripten bound classes need to be freed manually
}

interface InitResponse {
  type: 'init';
  error?: string;
}

interface TranslateResponse {
  type: 'translate';
  value?: string;
  error?: string;
  textToBraille?: number[];
  brailleToText?: number[];
}

interface BackTranslateMessage {
  type: 'backTranslate';
  tenjiString: string;
}

interface BackTranslateResponse {
  type: 'backTranslate';
  value?: string;
  error?: string;
}

interface TenjiModule {
  ToTenji(text: string): TranslateResult;
  ToTenjiWithOffsetMap(text: string, map: OffsetMap): TranslateResult;
  TenjiToHiragana(tenji: string): BackTranslateResult;
  OffsetMap: new() => OffsetMap;
}

declare global {
  interface Window {
    loadTenjiWasm(options: {wasmBinary: ArrayBuffer}): Promise<TenjiModule>;
  }
}

let postToParent: (msg: object) => void = (msg) =>
    window.parent.postMessage(msg, '*');

function initResponse(error?: string): InitResponse {
  return {type: 'init', error};
}

function translateResponse(
    value?: string, error?: string, textToBraille?: number[],
    brailleToText?: number[]): TranslateResponse {
  return {type: 'translate', value, error, textToBraille, brailleToText};
}

function errorToMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

let tenjiModule: TenjiModule|null = null;

async function handleInitMessage_(initMessage: InitMessage): Promise<void> {
  const tenjiData: TenjiData = initMessage.tenjiData;
  try {
    const indirectEval = eval;
    const wrapperJs = new TextDecoder().decode(tenjiData.wrapperJs);
    try {
      indirectEval(wrapperJs);
    } catch (error) {
      throw new Error(
          'Failed to evaluate Tenji wrapper JS: ' + errorToMessage(error));
    }

    const loadTenjiWasm = (globalThis as any).loadTenjiWasm;
    if (typeof loadTenjiWasm !== 'function') {
      throw new Error(
          'Failed to load Tenji WASM module: loadTenjiWasm is unavailable.');
    }

    try {
      tenjiModule = await loadTenjiWasm({wasmBinary: tenjiData.wasm});
    } catch (error) {
      throw new Error(
          'Failed to load Tenji WASM module: ' + errorToMessage(error));
    }
    postToParent(initResponse());
  } catch (error) {
    const errorMessage = errorToMessage(error);
    postToParent(initResponse(errorMessage));
  }
}

function handleTranslateMessage_(translateMessage: TranslateMessage): void {
  if (!tenjiModule) {
    console.error('Received translate request before Tenji module was loaded');
    postToParent(translateResponse(undefined, 'Tenji module not loaded'));
    return;
  }
  try {
    const text: string = translateMessage.text;

    // Step 1: Pre-compute boundaries for input text because Tenji deals in
    // UTF-8
    const inputBoundaries = StringUtil.buildUtf8OffsetTables(text);

    // Step 2: Initialize OffsetMap and seed it with the total number of UTF-8
    // input bytes
    const offsetMap = new tenjiModule.OffsetMap();
    // Emscripten instances must be explicitly freed to prevent memory leaks
    // in the sandbox
    let result: TranslateResult|null = null;
    try {
      offsetMap.Copy(inputBoundaries.totalBytes);

      // Step 3: Run translation while modifying the map in-place through the
      // bindings
      result = tenjiModule.ToTenjiWithOffsetMap(text, offsetMap);
      if (!result.ok) {
        throw new Error('Tenji translation failed');
      }

      // Step 4: Map the output cells to boundaries
      const outputBoundaries = StringUtil.buildUtf8OffsetTables(result.value);

      const textToBraille: number[] = [];
      const brailleToText: number[] = [];

      // Construct textToBraille (Length must equal text.length)
      for (let i = 0; i < text.length; i++) {
        // Find which UTF-8 byte this UTF-16 text index starts at:
        const srcByteIndex = inputBoundaries.utf16ToByte[i];
        // Consult map to find corresponding UTF-8 output byte
        const dstByteIndex = offsetMap.MapForward(srcByteIndex);
        // Find which UTF-16 JS index this mapped back to in output braille
        const brailleIndex = outputBoundaries.byteToUtf16[dstByteIndex];

        // Clamp to string length safeguard
        textToBraille.push(Math.min(brailleIndex, result.value.length));
      }

      // Construct brailleToText (Length must equal braille.length)
      // Since Tenji translation is monotonic, invert textToBraille directly.
      // This avoids OffsetMap.MapBack incorrectly mapping interior generated
      // bytes to the end of source spans. E.g., if textToBraille is [0, 2],
      // cells 0 and 1 belong to text 0, cell 2 belongs to text 1.
      let textIndex = 0;
      for (let brailleIndex = 0; brailleIndex < result.value.length;
           brailleIndex++) {
        // Advance textIndex if the NEXT text character starts at or before our
        // current braille cell.
        while (textIndex + 1 < text.length &&
               textToBraille[textIndex + 1] <= brailleIndex) {
          textIndex++;
        }
        brailleToText.push(textIndex);
      }

      postToParent(translateResponse(
          result.value, undefined, textToBraille, brailleToText));
    } finally {
      offsetMap.delete();
      result?.delete();
    }
  } catch (error) {
    const errorMessage = errorToMessage(error);
    postToParent(translateResponse(undefined, errorMessage));
  }
}

function handleBackTranslateMessage_(
    backTranslateMessage: BackTranslateMessage): void {
  if (!tenjiModule) {
    console.error(
        'Received backTranslate request before Tenji module was loaded');
    const response: BackTranslateResponse = {
      type: 'backTranslate',
      error: 'Tenji module not loaded',
    };
    postToParent(response);
    return;
  }
  try {
    const result =
        tenjiModule.TenjiToHiragana(backTranslateMessage.tenjiString);
    try {
      if (!result.ok) {
        throw new Error('Tenji back-translation failed');
      }
      const response:
          BackTranslateResponse = {type: 'backTranslate', value: result.value};
      postToParent(response);
    } finally {
      result.delete();
    }
  } catch (error) {
    const errorMessage = errorToMessage(error);
    const response:
        BackTranslateResponse = {type: 'backTranslate', error: errorMessage};
    postToParent(response);
  }
}

function handleMessage_(event: MessageEvent): void {
  if (!event.data) {
    return;
  }

  if (event.data.type === 'init') {
    void handleInitMessage_(event.data as InitMessage);
    return;
  }

  if (event.data.type === 'translate') {
    handleTranslateMessage_(event.data as TranslateMessage);
    return;
  }

  if (event.data.type === 'backTranslate') {
    handleBackTranslateMessage_(event.data as BackTranslateMessage);
    return;
  }

  console.warn(
      'Unknown message type received in sandboxed wrapper: ' + event.data.type);
}

globalThis.addEventListener('message', handleMessage_);

TestImportManager.exportForTesting(
    [
      'setTenjiModuleForTesting',
      (m: TenjiModule|null) => {
        tenjiModule = m;
      }
    ],
    [
      'setPostToParentForTesting',
      (fn: ((msg: object) => void)|null) => {
        postToParent = fn ?? ((msg) => window.parent.postMessage(msg, '*'));
      }
    ],
    ['handleSandboxMessageForTesting', handleMessage_],
);
