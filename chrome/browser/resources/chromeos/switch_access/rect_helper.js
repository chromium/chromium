// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** A collection of helper functions when dealing with rects. */
const RectHelper = {
  /** @type {!chrome.accessibilityPrivate.ScreenRect} */
  ZERO_RECT: {top: 0, left: 0, width: 0, height: 0},

  /**
   * @param {chrome.accessibilityPrivate.ScreenRect|undefined} rect
   * @return {number}
   */
  area: (rect) => rect ? rect.width * rect.height : 0,

  /**
   * Returns true if the two rects are equal.
   *
   * @param {chrome.accessibilityPrivate.ScreenRect=} rect1
   * @param {chrome.accessibilityPrivate.ScreenRect=} rect2
   * @return {boolean}
   */
  areEqual: (rect1, rect2) => {
    if (!rect1 && !rect2) {
      return true;
    }
    if (!rect1 || !rect2) {
      return false;
    }
    return rect1.left === rect2.left && rect1.top === rect2.top &&
        rect1.width === rect2.width && rect1.height === rect2.height;
  },

  /**
   * Finds the bottom of a rect.
   * @param {!chrome.accessibilityPrivate.ScreenRect} rect
   * @return {number}
   */
  bottom: (rect) => rect.top + rect.height,

  /**
   * Returns the point at the center of the rectangle.
   * @param {!chrome.accessibilityPrivate.ScreenRect} rect
   * @return {!{x: number, y: number}} an object containing the x and y
   *     coordinates of the center.
   */
  center: (rect) => {
    const x = rect.left + Math.round(rect.width / 2);
    const y = rect.top + Math.round(rect.height / 2);
    return {x, y};
  },

  /**
   * @param {chrome.accessibilityPrivate.ScreenRect} outer
   * @param {chrome.accessibilityPrivate.ScreenRect} inner
   * @return {boolean}
   */
  contains: (outer, inner) => {
    if (!outer || !inner) {
      return false;
    }
    return outer.left <= inner.left && outer.top <= inner.top &&
        RectHelper.right(outer) >= RectHelper.right(inner) &&
        RectHelper.bottom(outer) >= RectHelper.bottom(inner);
  },

  /**
   * @param {!chrome.accessibilityPrivate.ScreenRect} rect
   * @return {!chrome.accessibilityPrivate.ScreenRect}
   */
  deepCopy: (rect) => {
    const copy = (Object.assign({}, rect));
    return /** @type {!chrome.accessibilityPrivate.ScreenRect} */ (copy);
  },

  /**
   * Returns the largest rectangle contained within the outer rect that does not
   * overlap with the subtrahend (what is being subtracted).
   * @param {chrome.accessibilityPrivate.ScreenRect|undefined} outer
   * @param {chrome.accessibilityPrivate.ScreenRect|undefined} subtrahend
   * @return {chrome.accessibilityPrivate.ScreenRect|undefined}
   */
  difference: (outer, subtrahend) => {
    if (!outer || !subtrahend) {
      return outer;
    }

    if (outer.left >= RectHelper.right(subtrahend) ||
        RectHelper.right(outer) <= subtrahend.left ||
        outer.top >= RectHelper.bottom(subtrahend) ||
        RectHelper.bottom(outer) <= subtrahend.top) {
      // If the rectangles do not overlap, return the outer rect.
      return outer;
    }

    if (RectHelper.contains(subtrahend, outer)) {
      // If the subtrahend contains the outer rect, there is no region that does
      // not overlap. Return the zero rect.
      return RectHelper.ZERO_RECT;
    }

    let above, below, toTheLeft, toTheRight;

    if (outer.top < subtrahend.top) {
      above = {
        top: outer.top,
        left: outer.left,
        width: outer.width,
        height: (subtrahend.top - outer.top)
      };
    }

    if (RectHelper.bottom(outer) > RectHelper.bottom(subtrahend)) {
      below = {
        top: RectHelper.bottom(subtrahend),
        left: outer.left,
        width: outer.width,
        height: (RectHelper.bottom(outer) - RectHelper.bottom(subtrahend))
      };
    }

    if (outer.left < subtrahend.left) {
      toTheLeft = {
        top: outer.top,
        left: outer.left,
        width: (subtrahend.left - outer.left),
        height: outer.height
      };
    }

    if (RectHelper.right(outer) > RectHelper.right(subtrahend)) {
      toTheRight = {
        top: outer.top,
        left: RectHelper.right(subtrahend),
        width: (RectHelper.right(outer) - RectHelper.right(subtrahend)),
        height: outer.height
      };
    }

    // Of the four rects calculated above, find the one with the greatest area.
    const areaAbove = RectHelper.area(above);
    const areaBelow = RectHelper.area(below);
    const areaToTheLeft = RectHelper.area(toTheLeft);
    const areaToTheRight = RectHelper.area(toTheRight);

    if (areaAbove > areaBelow && areaAbove > areaToTheLeft &&
        areaAbove > areaToTheRight) {
      return above;
    }

    if (areaBelow > areaToTheLeft && areaBelow > areaToTheRight) {
      return below;
    }

    return areaToTheLeft > areaToTheRight ? toTheLeft : toTheRight;
  },

  /**
   * Increases the size of |outer| to entirely enclose |inner|, with |padding|
   * buffer on each side.
   * @param {number} padding
   * @param {chrome.accessibilityPrivate.ScreenRect=} outer
   * @param {chrome.accessibilityPrivate.ScreenRect=} inner
   * @return {chrome.accessibilityPrivate.ScreenRect|undefined}
   */
  expandToFitWithPadding: (padding, outer, inner) => {
    if (!outer || !inner) {
      return outer;
    }

    let newOuter = RectHelper.deepCopy(outer);

    if (newOuter.top > inner.top - padding) {
      newOuter.top = inner.top - padding;
      // The height should be the original bottom point less the new top point.
      newOuter.height = RectHelper.bottom(outer) - newOuter.top;
    }
    if (newOuter.left > inner.left - padding) {
      newOuter.left = inner.left - padding;
      // The new width should be the original right point less the new left.
      newOuter.width = RectHelper.right(outer) - newOuter.left;
    }
    if (RectHelper.bottom(newOuter) < RectHelper.bottom(inner) + padding) {
      newOuter.height = RectHelper.bottom(inner) + padding - newOuter.top;
    }
    if (RectHelper.right(newOuter) < RectHelper.right(inner) + padding) {
      newOuter.width = RectHelper.right(inner) + padding - newOuter.left;
    }

    return newOuter;
  },

  /**
   * @param {chrome.accessibilityPrivate.ScreenRect=} rect1
   * @param {chrome.accessibilityPrivate.ScreenRect=} rect2
   * @return {chrome.accessibilityPrivate.ScreenRect}
   */
  intersection: (rect1, rect2) => {
    if (!rect1 || !rect2) {
      return RectHelper.ZERO_RECT;
    }

    const left = Math.max(rect1.left, rect2.left);
    const top = Math.max(rect1.top, rect2.top);
    const right = Math.min(RectHelper.right(rect1), RectHelper.right(rect2));
    const bottom = Math.min(RectHelper.bottom(rect1), RectHelper.bottom(rect2));

    if (right <= left || bottom <= top) {
      return RectHelper.ZERO_RECT;
    }

    const width = right - left;
    const height = bottom - top;

    return {left, top, width, height};
  },

  /**
   * Finds the right of a rect.
   * @param {!chrome.accessibilityPrivate.ScreenRect} rect
   * @return {number}
   */
  right: (rect) => rect.left + rect.width,

  /*
   * @param {chrome.accessibilityPrivate.ScreenRect=} rect1
   * @param {chrome.accessibilityPrivate.ScreenRect=} rect2
   */
  sameRow: (rect1, rect2) => {
    if (!rect1 || !rect2) {
      return false;
    }
    const halfHeight = Math.round(rect1.height / 2);
    const topBound = rect1.top - halfHeight;
    const bottomBound = rect1.top + halfHeight;

    return topBound <= rect2.top && bottomBound >= rect2.top;
  },

  /**
   * Returns a string representing the given rectangle.
   * @param {chrome.accessibilityPrivate.ScreenRect|undefined} rect
   * @return {string}
   */
  toString: (rect) => {
    let str = '';
    if (rect) {
      str = rect.left + ',' + rect.top + ' ';
      str += rect.width + 'x' + rect.height;
    }
    return str;
  },

  /**
   * Returns the union of the specified rectangles.
   * @param {!chrome.accessibilityPrivate.ScreenRect} rect1
   * @param {!chrome.accessibilityPrivate.ScreenRect} rect2
   * @return {!chrome.accessibilityPrivate.ScreenRect}
   */
  union: (rect1, rect2) => {
    const top = rect1.top < rect2.top ? rect1.top : rect2.top;
    const left = rect1.left < rect2.left ? rect1.left : rect2.left;

    const r1Bottom = RectHelper.bottom(rect1);
    const r2Bottom = RectHelper.bottom(rect2);
    const bottom = r1Bottom > r2Bottom ? r1Bottom : r2Bottom;

    const r1Right = RectHelper.right(rect1);
    const r2Right = RectHelper.right(rect2);
    const right = r1Right > r2Right ? r1Right : r2Right;

    const height = bottom - top;
    const width = right - left;

    return {top, left, width, height};
  },

  /**
   * Returns the union of all the rectangles specified.
   * @param {!Array<!chrome.accessibilityPrivate.ScreenRect>} rects
   * @return {!chrome.accessibilityPrivate.ScreenRect}
   */
  unionAll: (rects) => {
    if (rects.length < 1) {
      return RectHelper.ZERO_RECT;
    }

    let result = rects[0];
    for (let i = 1; i < rects.length; i++) {
      result = RectHelper.union(result, rects[i]);
    }
    return result;
  }
};
