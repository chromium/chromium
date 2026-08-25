// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The production CompositionCandidateProvider for Japanese
 * kana-to-kanji conversion (see composition_candidate_provider.ts), backed by
 * the open-source Mozc engine compiled to WASM directly into the same
 * combined module Tenji's own translation library uses (see
 * sandboxed_tenji_wrapper.ts's MozcConvertHiraganaToKanji binding).
 *
 * Kana only ever reaches conversion after back-translation has produced it,
 * so TenjiTranslator has always already triggered (or is in the middle of)
 * the install-and-start-worker sequence for that same WASM module by the time
 * getCandidates() is called. WasmKanaKanjiProvider therefore doesn't repeat
 * that sequence; it calls TenjiTranslator.init() itself, which resolves to
 * the same cached promise TenjiTranslator's own callers are already
 * awaiting.
 */

import {TestImportManager} from '/common/testing/test_import_manager.js';

import {OffscreenBridge} from '../../common/offscreen_bridge.js';

import {CompositionCandidateProvider} from './composition_candidate_provider.js';
import {TenjiTranslator} from './tenji_translator.js';

/** @return Whether |text| contains at least one hiragana character. */
export function containsHiragana(text: string): boolean {
  return /[ぁ-ゖ]/.test(text);
}

/**
 * Regular expression that matches trailing word separators (ASCII space or
 * ideographic space) produced by a blank braille cell, which need to be
 * trimmed before treating the remaining text as a reading to convert.
 */
const TRAILING_SEPARATOR_RE = /[ 　]+$/;

/** The maximum number of candidates requested from the Mozc engine. */
const MAX_CANDIDATES = 20;

/**
 * The production CompositionCandidateProvider, backed by the Mozc engine
 * compiled into the same WASM module Tenji's own translation library uses.
 * Shares a single install-and-start-worker path with TenjiTranslator (see
 * the fileoverview above) rather than triggering its own.
 */
export class WasmKanaKanjiProvider implements CompositionCandidateProvider {
  async getCandidates(input: string): Promise<string[]> {
    const kana = input.replace(TRAILING_SEPARATOR_RE, '');
    if (!containsHiragana(kana)) {
      return [];
    }

    if (!await new TenjiTranslator().init()) {
      return [];
    }

    try {
      return await OffscreenBridge.tenjiConvert(kana, MAX_CANDIDATES);
    } catch (error) {
      console.error('Error during Mozc conversion: ' + error);
      return [];
    }
  }
}

TestImportManager.exportForTesting(
    WasmKanaKanjiProvider, ['containsHiragana', containsHiragana]);
