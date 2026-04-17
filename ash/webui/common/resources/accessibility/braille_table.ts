// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Represents a braille table used in ChromeVox, either from liblouis or a
 * custom table.
 */
export interface BrailleTable {
  locale: string;
  dots: string;
  id: string;
  grade?: string;
  variant?: string;
  fileNames: string;  // mandatory for legacy liblouis assumptions
  enDisplayName?: string;
  alwaysUseEnDisplayName: boolean;
}

/**
 * The custom table for Japanese braille (Tenji), that is not part of the
 * liblouis tables.
 */
export const JP_BRAILLE_TENJI_TABLE: BrailleTable = {
  id: 'ja-tenji',
  locale: 'ja',
  dots: '6',
  grade: '1',
  fileNames: '',
  enDisplayName: 'Japanese braille (Tenji)',
  alwaysUseEnDisplayName: false,
};
