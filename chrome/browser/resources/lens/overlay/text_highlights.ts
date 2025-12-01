// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import type {CenterRotatedBox} from './geometry.mojom-webui.js';
import {WritingDirection} from './text.mojom-webui.js';
import type {TextResponse} from './text_rendering.js';

// Percentage of allowance for the difference in top positions of two
// consecutive word boxes.
const DELTA_TOP_ALLOWANCE = 0.2;
// Percentage of allowance for the difference in width positions of two
// consecutive word boxes. This value is used to decide whether to coalesce two
// bounding boxes that are next to each other horizontally.
const DELTA_WIDTH_ALLOWANCE = 0.75;
// Allowance for the difference in angle positions of two consecuitve word boxes
// in radians. This value is used to decide whether to coalesce two bounding
// boxes that have different rotations.
const DELTA_ANGLE_ALLOWANCE = 0.002;

// Shared interface representing a highlighted lines. All values are normalized
// relative to selection overlay.
export interface HighlightedLine {
  height: number;
  left: number;
  top: number;
  width: number;
  rotation: number;
}

// Create highlighted lines to be rendered in the text layer. Creates these
// highlighted lines based on the text from `startIndex` to `endIndex` in
// `text.receivedWords`.
export function createHighlightedLines(
    text: TextResponse, startIndex: number,
    endIndex: number): HighlightedLine[] {
  const words = text.receivedWords;
  if (words.length <= 0) {
    return [];
  }
  assert(startIndex >= 0);
  assert(endIndex >= 0);
  assert(endIndex < words.length);
  const highlightedLines: HighlightedLine[] = [];

  for (let i = startIndex; i <= endIndex; i++) {
    const firstWord = words[i];
    assert(firstWord);
    assert(firstWord.geometry);
    let box: CenterRotatedBox = structuredClone(firstWord.geometry.boundingBox);
    // Loop through the rest of the bounding boxes in this line to make one
    // large bounding box for the entire line until a word that should not be
    // colesced is reached.
    for (let j = i + 1; j <= endIndex; j++) {
      const nextWord = words[j];
      assert(nextWord);
      assert(nextWord.geometry);

      if (!shouldCoalesce(box, nextWord.geometry.boundingBox, text, i, j)) {
        break;
      }
      // union `box` with nextWord rect.
      const newWordBoundingBox = nextWord.geometry.boundingBox;
      box = getUnionRect(box, newWordBoundingBox);
      i = j;
    }

    highlightedLines.push({
      height: box.box.height,
      left: box.box.x - box.box.width / 2,
      top: box.box.y - box.box.height / 2,
      width: box.box.width,
      rotation: box.rotation,
    });
  }
  return highlightedLines;
}

// Whether `box` should be colesced with `nextBox` to create a unified box for
// highlights.
function shouldCoalesce(
    box: CenterRotatedBox, nextBox: CenterRotatedBox, text: TextResponse,
    wordIndex: number, nextIndex: number): boolean {
  if (!sameLineAndParagraph(text, wordIndex, nextIndex)) {
    return false;
  }

  const deltaAngle = Math.abs(box.rotation - nextBox.rotation);
  if (deltaAngle >= DELTA_ANGLE_ALLOWANCE) {
    return false;
  }

  const wordLeft = box.box.x - box.box.width / 2;
  const wordTop = box.box.y - box.box.height / 2;

  const nextWordLeft = nextBox.box.x - nextBox.box.width / 2;
  const nextWordTop = nextBox.box.y - nextBox.box.height / 2;

  const word = text.receivedWords[wordIndex];
  const isVerticalWritingDirection =
      word.writingDirection === WritingDirection.kTopToBottom;

  const deltaTop = isVerticalWritingDirection ?
      Math.abs(
          (nextWordLeft + nextBox.box.width) - (wordLeft + box.box.width)) :
      Math.abs(nextWordTop - wordTop);
  const maxHeightOrWidth = isVerticalWritingDirection ?
      Math.max(box.box.width, nextBox.box.width) :
      Math.max(box.box.height, nextBox.box.height);
  const deltaTopAllowance = DELTA_TOP_ALLOWANCE * maxHeightOrWidth;
  if (deltaTop >= deltaTopAllowance) {
    return false;
  }

  const minLeftOrTop = isVerticalWritingDirection ?
      Math.min(wordTop, nextWordTop) :
      Math.min(wordLeft, nextWordLeft);
  const maxRightOrBottom = isVerticalWritingDirection ?
      Math.max(wordTop + box.box.height, nextWordTop + nextBox.box.height) :
      Math.max(wordLeft + box.box.width, nextWordLeft + nextBox.box.width);
  const unionWidth = maxRightOrBottom - minLeftOrTop;
  const sumWidth = isVerticalWritingDirection ?
      box.box.height + nextBox.box.height :
      box.box.width + nextBox.box.width;
  return sumWidth / unionWidth >= DELTA_WIDTH_ALLOWANCE;
}

// Returns whether the words at `wordIndex` and `nextIndex` in `text` are in the
// same line and paragraph.
function sameLineAndParagraph(
    text: TextResponse, wordIndex: number, nextIndex: number): boolean {
  return text.paragraphNumbers[wordIndex] ===
      text.paragraphNumbers[nextIndex] &&
      text.lineNumbers[wordIndex] === text.lineNumbers[nextIndex];
}

// Provides a new center rotated box that is a union of `box` and `nextBox`
// provided. Must be the same coordinate type. Currently has no support for
// unions of rotated boxes.
function getUnionRect(
    box: CenterRotatedBox, nextBox: CenterRotatedBox): CenterRotatedBox {
  assert(box.coordinateType === nextBox.coordinateType);
  const wordLeft = box.box.x - box.box.width / 2;
  const wordTop = box.box.y - box.box.height / 2;
  const wordRight = box.box.x + box.box.width / 2;
  const wordBottom = box.box.y + box.box.height / 2;

  const nextWordLeft = nextBox.box.x - nextBox.box.width / 2;
  const nextWordTop = nextBox.box.y - nextBox.box.height / 2;
  const nextWordRight = nextBox.box.x + nextBox.box.width / 2;
  const nextWordBottom = nextBox.box.y + nextBox.box.height / 2;

  const unionLeft = Math.min(wordLeft, nextWordLeft);
  const unionRight = Math.max(wordRight, nextWordRight);
  const unionTop = Math.min(wordTop, nextWordTop);
  const unionBottom = Math.max(wordBottom, nextWordBottom);

  const unionWidth = unionRight - unionLeft;
  const unionHeight = unionBottom - unionTop;
  const centerX = unionLeft + unionWidth / 2;
  const centerY = unionTop + unionHeight / 2;
  return {
    box: {
      x: centerX,
      y: centerY,
      height: unionHeight,
      width: unionWidth,
    },
    rotation: box.rotation,
    coordinateType: box.coordinateType,
  };
}
