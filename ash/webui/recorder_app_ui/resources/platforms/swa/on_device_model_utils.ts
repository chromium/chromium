// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains all utility functions for on_device_model.ts.

import {LanguageCode} from '../../core/soda/language_info.js';
import {assertExists} from '../../core/utils/assert.js';

/**
 * Parse model response.
 */
export function parseResponse(res: string): string {
  // Note this is NOT an underscore: ▁(U+2581)
  return res.replaceAll('▁', ' ').replaceAll(/\n+/g, '\n').trim();
}

/**
 * Detect whether model returns canned response.
 */
export function isCannedResponse(response: string): boolean {
  // Model could return canned response in various formats. e.g. "Sorry, I
  // am a large language model ..." or "Sorry, I’m a text-based AI ...".
  // Capture the most common leading phrase.
  const commonLeadingCannedPhrases = [
    'Sorry, I’m a',
    'Sorry, I am a',
  ];
  return commonLeadingCannedPhrases.some(
    (phrase) => response.trimStart().startsWith(phrase),
  );
}

/**
 * Detect invalid format model response, e.g. invalid bullet point format
 * or unexpected bullet point amount.
 */
export function isInvalidFormatResponse(
  response: string,
  expectedBulletPointCount: number,
): boolean {
  const bulletPoints = response.split('\n');

  // Normally happens when model response is interrupted.
  if (bulletPoints.length !== expectedBulletPointCount) {
    return true;
  }

  // All valid bullet points should start with "- ".
  return bulletPoints.some(
    (bulletPoint) => !bulletPoint.trimStart().startsWith('- '),
  );
}

/**
 * Trim 0-word and repeated bullet points.
 */
export function trimRepeatedBulletPoints(
  response: string,
  maxLength: number,
  language: LanguageCode,
  lcsScoreThreshold: number,
): string[] {
  const bulletPoints = response.split('\n');
  // To preserve longest one if repetition happens, sort by length descending.
  bulletPoints.sort((b1, b2) => b2.length - b1.length);
  const repeatedIndexSet = getRepeatedBulletPointIndexes(
    bulletPoints,
    maxLength,
    lcsScoreThreshold,
    language,
  );

  // Remove 0-word and repeated bullet point.
  return bulletPoints.filter(
    (point, index) => (segmentStringToWords(point, language).length > 0) &&
      !repeatedIndexSet.has(index),
  );
}

// Return the set of indexes of repeated bullet point.
function getRepeatedBulletPointIndexes(
  bulletPoints: string[],
  maxLength: number,
  lcsScoreThreshold: number,
  language: LanguageCode,
): Set<number> {
  const repeatedIndexSet = new Set<number>();
  for (const [i, bulletPoint] of bulletPoints.entries()) {
    if (repeatedIndexSet.has(i)) {
      continue;
    }

    // If one bullet point is too long, normally it contains repetition like
    // aaaa..... We don't count max length by word since it might repeating in
    // one word.
    if (bulletPoint.length > maxLength) {
      repeatedIndexSet.add(i);
      continue;
    }

    // Compare with previous bullet points and mark repeated at current
    // bullet points because normally model will generate repetition in later
    // bullet points.
    for (let j = i - 1; j >= 0; j--) {
      if (repeatedIndexSet.has(j)) {
        continue;
      }

      const otherBulletPoint = assertExists(bulletPoints[j]);

      if (getLcsScore(bulletPoint, otherBulletPoint, language) >
          lcsScoreThreshold) {
        repeatedIndexSet.add(i);
        break;
      }
    }
  }

  return repeatedIndexSet;
}

/* eslint-disable  @typescript-eslint/no-non-null-assertion */
/**
 * Typescript cannot identify type correctly when iterate array by index.
 */
/**
 * Find LCS (longest common subsequence) by word between str1 and str2.
 * Return score by dividing the length of the shorter comparison string.
 * E.g. "hello world yo" "hello wor yo" -> score = 0.66 (LCS is "hello yo")
 * We don't count LCS by character because it doesn't make sense for syntax
 * meaning.
 * WARNING: The time complexity is O(n^2). Be careful about the input size.
 */
function getLcsScore(str1: string, str2: string, language: LanguageCode):
  number {
  const words1 = segmentStringToWords(str1, language);
  const words2 = segmentStringToWords(str2, language);

  const len1 = words1.length;
  const len2 = words2.length;

  if (len1 === 0 || len2 === 0) {
    return 0;
  }

  const dp = Array.from(
    {length: len1 + 1},
    () => Array.from({length: len2 + 1}, () => 0),
  );

  for (let i = 1; i <= len1; i++) {
    for (let j = 1; j <= len2; j++) {
      if (words1[i - 1] === words2[j - 1]) {
        dp[i]![j] = dp[i - 1]![j - 1]! + 1;
      } else {
        dp[i]![j] = Math.max(dp[i - 1]![j]!, dp[i]![j - 1]!);
      }
    }
  }

  return dp[len1]![len2]! / Math.min(len1, len2);
}
/* eslint-enable @typescript-eslint/no-non-null-assertion */

// Segment string to words and return word-like segments only.
function segmentStringToWords(str: string, language: LanguageCode): string[] {
  const segmenter = new Intl.Segmenter(language, {granularity: 'word'});
  const segments = segmenter.segment(str);
  const words = [];
  for (const segment of segments) {
    if (segment.isWordLike === true) {
      words.push(segment.segment);
    }
  }
  return words;
}
