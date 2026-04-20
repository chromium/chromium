// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  return (
      (point >= 0) &&
      ((point <= window.innerHeight) ||
       (point <= document.documentElement.clientHeight)));
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
      Array.from(new Set(unsortedRects)).sort((a, b) => a.bottom - b.bottom);
  const combinedRects: DOMRect[] = [sortedRects[0]!];
  // The smaller the line spacing, the larger the threshold needs to be, since
  // it is more likely for lines to have overlapping bounds. Thus, invert the
  // line spacing value and multiply by 10 to ensure it is above 1.
  const lineHeight =
      chrome.readingMode.getLineSpacingValue(chrome.readingMode.lineSpacing);
  const threshold =
      Math.max(1, chrome.readingMode.fontSize) * (1 / lineHeight) * 10;

  for (let i = 1; i < sortedRects.length; i++) {
    const currentRect = sortedRects[i]!;
    const lastRect = combinedRects[combinedRects.length - 1]!;

    // The rects are sorted by their bottom values. If the current rect top is
    // above the previous rect top, then it encompasses the previous line (or
    // more), so this rect is not a single line of text.
    if (currentRect.top < lastRect.top) {
      continue;
    }

    // Skip duplicate rects.
    if (lastRect.top === currentRect.top &&
        lastRect.bottom === currentRect.bottom) {
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
      combinedRects.pop();
    }
    combinedRects.push(currentRect);
  }

  return combinedRects;
}
