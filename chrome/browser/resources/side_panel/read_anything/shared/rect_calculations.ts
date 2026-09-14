// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VisualBrowserProxyImpl} from '../app/visual_browser_proxy.js';

// The percent of a view that must be visible to be considered "mostly visible"
// for the purpose of determining what's likely being actually read in the
// reading mode panel.
export const MOSTLY_VISIBLE_PERCENT = 0.8;

// Returns true if the given rect is mostly within the visible window.
export function isRectMostlyVisible(rect: DOMRect): boolean {
  if (rect.height <= 0) {
    return false;
  }
  const isTopMostlyVisible = isPointVisible(rect.top) &&
      isPointVisible(rect.top + (rect.height * MOSTLY_VISIBLE_PERCENT));
  const isBottomMostlyVisible = isPointVisible(rect.bottom) &&
      isPointVisible(rect.bottom - (rect.height * MOSTLY_VISIBLE_PERCENT));
  const isMiddleMostlyVisible = rect.top < 0 &&
      rect.bottom > window.innerHeight &&
      (rect.height * MOSTLY_VISIBLE_PERCENT) < window.innerHeight;
  return isTopMostlyVisible || isBottomMostlyVisible || isMiddleMostlyVisible;
}

// Returns true if any part of the given rect is within the visible window.
export function isRectVisible(rect: DOMRect): boolean {
  return (rect.height > 0) &&
      ((rect.top <= 0 && rect.bottom >= window.innerHeight) ||
       isPointVisible(rect.top) || isPointVisible(rect.bottom));
}

function isPointVisible(point: number) {
  return (point >= 0) && (point <= window.innerHeight);
}

// Recalculates line positions based on the container and height.
export function calculateTextBounds(container: HTMLElement, height: number):
    {minY: number, maxY: number, bounds: DOMRect[]} {
  const range = document.createRange();
  range.selectNodeContents(container);
  const bounds = combineIntersectingRects(Array.from(range.getClientRects()));
  return {minY: container.offsetTop, maxY: height, bounds};
}

// Merges intersecting rects that are within a certain threshold.
function combineIntersectingRects(unsortedRects: DOMRect[]): DOMRect[] {
  if (unsortedRects.length === 0) {
    return [];
  }

  const sortedRects =
      unsortedRects.sort((a, b) => a.bottom - b.bottom);
  const combinedRects: DOMRect[] = [sortedRects[0]!];
  // The smaller the line spacing, the larger the threshold needs to be, since
  // it is more likely for lines to have overlapping bounds. Thus, invert the
  // line spacing value and multiply by 10 to ensure it is above 1.
  const visualBrowserProxy = VisualBrowserProxyImpl.getInstance();
  const lineHeight = visualBrowserProxy.getLineSpacingValue(
      visualBrowserProxy.getLineSpacing());
  const threshold =
      Math.max(1, visualBrowserProxy.getFontSize()) * (1 / lineHeight) * 10;

  for (let i = 1; i < sortedRects.length; i++) {
    const currentRect = sortedRects[i]!;
    const lastRect = combinedRects[combinedRects.length - 1]!;

    // If the rects have nearly identical top and bottom, they are on the same
    // line (e.g. side-by-side words). Merge them into a single rect.
    if (Math.abs(lastRect.top - currentRect.top) < 2 &&
        Math.abs(lastRect.bottom - currentRect.bottom) < 2) {
      combinedRects[combinedRects.length - 1] =
          mergeRects(lastRect, currentRect);
      continue;
    }

    // The rects are sorted by their bottom values. If the current rect top is
    // above the previous rect top, then it encompasses the previous line (or
    // more), so this rect is not a single line of text.
    if (currentRect.top < lastRect.top) {
      continue;
    }

    // If the next rect intersects with the last rect, and the intersection is
    // larger than a threshold, merge them by removing the last rect and
    // keeping the new one with a higher bottom value. The threshold is > 0
    // because some fonts may cause their returned rects to slightly overlap,
    // even though the lines are visually distinct.
    const isIntersecting = lastRect.bottom > currentRect.top &&
        lastRect.bottom <= currentRect.bottom;
    if (isIntersecting && (lastRect.bottom - currentRect.top) > threshold) {
      combinedRects[combinedRects.length - 1] =
          mergeRects(lastRect, currentRect);
    } else {
      combinedRects.push(currentRect);
    }
  }

  return combinedRects;
}

function mergeRects(rect1: DOMRect, rect2: DOMRect) {
  return new DOMRect(
      Math.min(rect1.left, rect2.left), Math.min(rect1.top, rect2.top),
      Math.max(rect1.right, rect2.right) - Math.min(rect1.left, rect2.left),
      Math.max(rect1.bottom, rect2.bottom) - Math.min(rect1.top, rect2.top));
}

// Returns the most common vertical distance between consecutive lines.
// "Pitch" is the distance from the top of one line to the top of the next,
// effectively representing the line height plus line spacing.
export function getMostCommonPitch(bounds: DOMRect[]): number {
  if (bounds.length === 0) {
    return 0;
  }
  if (bounds.length === 1) {
    return Number(bounds[0]!.height.toFixed(1));
  }

  const modeResult = bounds.reduce((acc, rect, i) => {
    if (i < bounds.length - 1) {
      const nextRect = bounds[i + 1]!;
      const pitch = Number((nextRect.top - rect.top).toFixed(1));
      const pCount = (acc.pitches.get(pitch) || 0) + 1;
      acc.pitches.set(pitch, pCount);
      if (pCount > acc.maxPCount) {
        acc.maxPCount = pCount;
        acc.modePitch = pitch;
      }
    }
    return acc;
  }, {pitches: new Map<number, number>(), maxPCount: 0, modePitch: 0});

  return modeResult.modePitch;
}

// Binary searches for the first rect whose bottom is >= y.
// Returns the index of that rect, or rects.length if all rects have bottom < y.
function findFirstRectAtY(rects: DOMRect[], y: number): number {
  let low = 0;
  let high = rects.length - 1;
  let targetIndex = rects.length;

  while (low <= high) {
    const mid = (low + high) >> 1;
    if (rects[mid]!.bottom >= y) {
      targetIndex = mid;
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }

  return targetIndex;
}

// Returns the index of the first rect in the given list that matches the
// given y position.
export function getRectIndexAtY(
    y: number, rects: DOMRect[], isForward: boolean): number {
  if (rects.length === 0) {
    return -1;
  }

  const targetIndex = findFirstRectAtY(rects, y);
  if (targetIndex >= rects.length) {
    return rects.length - 1;
  }

  return (isForward || targetIndex === 0) ? targetIndex : targetIndex - 1;
}

// Finds the line in `lines` with the greatest vertical overlap with `wordRect`.
// If no line directly overlaps, finds the line closest to the vertical midpoint
// of `wordRect`.
export function getLineForRect(wordRect: DOMRect, lines: DOMRect[]): DOMRect|
    null {
  if (lines.length === 0) {
    return null;
  }

  const wordCenterY = (wordRect.top + wordRect.bottom) / 2;
  const candidateIndex =
      getRectIndexAtY(wordCenterY, lines, /*isForward=*/ true);
  if (candidateIndex < 0 || candidateIndex >= lines.length) {
    return null;
  }

  // Check candidate line and adjacent neighbors to pick the line with
  // maximum vertical overlap (e.g. for superscripts that may cross line
  // boundaries).
  let bestLine = lines[candidateIndex]!;
  let maxOverlap = 0;
  const start = Math.max(0, candidateIndex - 1);
  const end = Math.min(lines.length - 1, candidateIndex + 1);
  for (let i = start; i <= end; i++) {
    const line = lines[i]!;
    const overlapTop = Math.max(wordRect.top, line.top);
    const overlapBottom = Math.min(wordRect.bottom, line.bottom);
    const overlap = Math.max(0, overlapBottom - overlapTop);
    if (overlap > maxOverlap) {
      maxOverlap = overlap;
      bestLine = line;
    }
  }

  if (maxOverlap > 0) {
    return bestLine;
  }

  // Fallback: If no line directly overlaps vertically, compare adjacent
  // candidate lines closest to the word's vertical midpoint.
  let minDistance = Infinity;
  for (let i = start; i <= end; i++) {
    const line = lines[i]!;
    const lineCenterY = (line.top + line.bottom) / 2;
    const distance = Math.abs(wordCenterY - lineCenterY);
    if (distance < minDistance) {
      minDistance = distance;
      bestLine = line;
    }
  }
  return bestLine;
}
