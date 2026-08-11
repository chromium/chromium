// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface for providing composition-input conversion
 * candidates for languages that enter text as an IME composition to be
 * converted before commit.
 *
 * The conversion flow in ChromeVox braille input is abstracted behind this
 * interface, following the same naming as the translator-level
 * `usesCompositionInput` flag it pairs with (see braille_translator.ts). This
 * interface is intentionally language-agnostic; implementations are specific
 * to whatever conversion a given translator needs, so each translator that
 * opts into composition input supplies its own provider rather than
 * depending on one written for another translator's language.
 */

/** Provides conversion candidates for composition-input text. */
export interface CompositionCandidateProvider {
  /**
   * Returns conversion candidates for |input|, the composition text entered
   * so far. Returns an empty array if no conversion is available, in which
   * case the input should be committed as-is.
   */
  getCandidates(input: string): Promise<string[]>;
}
