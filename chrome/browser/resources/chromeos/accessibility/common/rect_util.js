// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Equivalent to chrome.accessibilityPrivate.ScreenRect.
 * @typedef {{top: number, left: number, width: number, height: number}}
 */
let ScreenRect;

/** A collection of helper functions when dealing with rects. */
export const RectUtil = {
  /** @type {!ScreenRect} */
  ZERO_RECT: {top: 0, left: 0, width: 0, height: 0},

  /**
   * Return the rect that encloses two points.
   * @param {number} x1 The first x coordinate.
   * @param {number} y1 The first y coordinate.
   * @param {number} x2 The second x coordinate.
   * @param {number} y2 The second x coordinate.
   * @return {!ScreenRect}
   */
  rectFromPoints: (x1, y1, x2, y2) => {
    const left = Math.min(x1, x2);
    const right = Math.max(x1, x2);
    const top = Math.min(y1, y2);
    const bottom = Math.max(y1, y2);
    const width = right - left;
    const height = bottom - top;
    return {left, top, width, height};
  },

  /**
   * @param {!ScreenRect} rect1
   * @param {!ScreenRect} rect2
   * @return {boolean}
   */
  adjacent: (rect1, rect2) => {
    const verticallyStacked = rect1.top === RectUtil.bottom(rect2) ||
        RectUtil.bottom(rect1) === rect2.top;
    const horizontallyStacked = rect1.left === RectUtil.right(rect2) ||
        RectUtil.right(rect1) === rect2.left;

    const verticallyOverlap =
        (rect1.top >= rect2.top && rect1.top <= RectUtil.bottom(rect2)) ||
        (rect2.top >= rect1.top && rect2.top <= RectUtil.bottom(rect1));
    const horizontallyOverlap =
        (rect1.left >= rect2.left && rect1.left <= RectUtil.right(rect2)) ||
        (rect2.left >= rect1.left && rect2.left <= RectUtil.right(rect1));

    return (verticallyStacked && horizontallyOverlap) ||
        (horizontallyStacked && verticallyOverlap);
  },

  /**
   * @param {ScreenRect|undefined} rect
   * @return {number}
   */
  area: rect => (rect ? rect.width * rect.height : 0),

  /**
   * Finds the bottom of a rect.
   * @param {!ScreenRect} rect
   * @return {number}
   */
  bottom: rect => rect.top + rect.height,

  /**
   * Returns the point at the center of the rectangle.
   * @param {!ScreenRect} rect
   * @return {!{x: number, y: number}} an object containing the x and y
   *     coordinates of the center.
   */
  center: rect => {
    const x = rect.left + Math.round(rect.width / 2);
    const y = rect.top + Math.round(rect.height / 2);
    return {x, y};
  },

  /**
   * Checks if the two specified rectangles are within at most a specified
   * distance of each other both horizontally and vertically.
   * @param {ScreenRect} rect1
   * @param {ScreenRect} rect2
   * @param {number} tolerance
   * @return {boolean}
   */
  close: (rect1, rect2, tolerance) => {
    const maxLeft = rect1.left > rect2.left ? rect1.left : rect2.left;
    const minRight = RectUtil.right(rect1) < RectUtil.right(rect2) ?
        RectUtil.right(rect1) :
        RectUtil.right(rect2);
    const maxTop = rect1.top > rect2.top ? rect1.top : rect2.top;
    const minBottom = RectUtil.bottom(rect1) < RectUtil.bottom(rect2) ?
        RectUtil.bottom(rect1) :
        RectUtil.bottom(rect2);

    // Negative values indicate the rectangles overlap in that dimension.
    return maxLeft - minRight <= tolerance && maxTop - minBottom <= tolerance;
  },

  /**
   * @param {ScreenRect} outer
   * @param {ScreenRect} inner
   * @return {boolean}
   */
  contains: (outer, inner) => {
    if (!outer || !inner) {
      return false;
    }
    return outer.left <= inner.left && outer.top <= inner.top &&
        RectUtil.right(outer) >= RectUtil.right(inner) &&
        RectUtil.bottom(outer) >= RectUtil.bottom(inner);
  },

  /**
   * @param {!ScreenRect} rect
   * @return {!ScreenRect}
   */
  deepCopy: rect => /** @type {!ScreenRect} */ (Object.assign({}, rect)),

  /**
   * Returns the largest rectangle contained within the outer rect that does not
   * overlap with the subtrahend (what is being subtracted).
   * @param {ScreenRect|undefined} outer
   * @param {ScreenRect|undefined} subtrahend
   * @return {ScreenRect|undefined}
   */
  difference: (outer, subtrahend) => {
    if (!outer || !subtrahend) {
      return outer;
    }

    if (!RectUtil.overlaps(outer, subtrahend)) {
      // If the rectangles do not overlap, return the outer rect.
      return outer;
    }

    if (RectUtil.contains(subtrahend, outer)) {
      // If the subtrahend contains the outer rect, there is no region that does
      // not overlap. Return the zero rect.
      return RectUtil.ZERO_RECT;
    }

    let above;
    let below;
    let toTheLeft;
    let toTheRight;

    if (outer.top < subtrahend.top) {
      above = {
        top: outer.top,
        left: outer.left,
        width: outer.width,
        height: (subtrahend.top - outer.top),
      };
    }

    if (RectUtil.bottom(outer) > RectUtil.bottom(subtrahend)) {
      below = {
        top: RectUtil.bottom(subtrahend),
        left: outer.left,
        width: outer.width,
        height: (RectUtil.bottom(outer) - RectUtil.bottom(subtrahend)),
      };
    }

    if (outer.left < subtrahend.left) {
      toTheLeft = {
        top: outer.top,
        left: outer.left,
        width: (subtrahend.left - outer.left),
        height: outer.height,
      };
    }

    if (RectUtil.right(outer) > RectUtil.right(subtrahend)) {
      toTheRight = {
        top: outer.top,
        left: RectUtil.right(subtrahend),
        width: (RectUtil.right(outer) - RectUtil.right(subtrahend)),
        height: outer.height,
      };
    }

    // Of the four rects calculated above, find the one with the greatest area.
    const areaAbove = RectUtil.area(above);
    const areaBelow = RectUtil.area(below);
    const areaToTheLeft = RectUtil.area(toTheLeft);
    const areaToTheRight = RectUtil.area(toTheRight);

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
   * Returns true if the two rects are equal.
   *
   * @param {ScreenRect=} rect1
   * @param {ScreenRect=} rect2
   * @return {boolean}
   */
  equal: (rect1, rect2) => {
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
   * Increases the size of |outer| to entirely enclose |inner|, with |padding|
   * buffer on each side.
   * @param {number} padding
   * @param {ScreenRect=} outer
   * @param {ScreenRect=} inner
   * @return {ScreenRect|undefined}
   */
  expandToFitWithPadding: (padding, outer, inner) => {
    if (!outer || !inner) {
      return outer;
    }

    const newOuter = RectUtil.deepCopy(outer);

    if (newOuter.top > inner.top - padding) {
      newOuter.top = inner.top - padding;
      // The height should be the original bottom point less the new top point.
      newOuter.height = RectUtil.bottom(outer) - newOuter.top;
    }
    if (newOuter.left > inner.left - padding) {
      newOuter.left = inner.left - padding;
      // The new width should be the original right point less the new left.
      newOuter.width = RectUtil.right(outer) - newOuter.left;
    }
    if (RectUtil.bottom(newOuter) < RectUtil.bottom(inner) + padding) {
      newOuter.height = RectUtil.bottom(inner) + padding - newOuter.top;
    }
    if (RectUtil.right(newOuter) < RectUtil.right(inner) + padding) {
      newOuter.width = RectUtil.right(inner) + padding - newOuter.left;
    }

    return newOuter;
  },

  /**
   * @param {ScreenRect=} rect1
   * @param {ScreenRect=} rect2
   * @return {ScreenRect}
   */
  intersection: (rect1, rect2) => {
    if (!rect1 || !rect2) {
      return RectUtil.ZERO_RECT;
    }

    const left = Math.max(rect1.left, rect2.left);
    const top = Math.max(rect1.top, rect2.top);
    const right = Math.min(RectUtil.right(rect1), RectUtil.right(rect2));
    const bottom = Math.min(RectUtil.bottom(rect1), RectUtil.bottom(rect2));

    if (right <= left || bottom <= top) {
      return RectUtil.ZERO_RECT;
    }

    const width = right - left;
    const height = bottom - top;

    return {left, top, width, height};
  },

  /**
   * Returns true if |rect1| and |rect2| overlap.
   * @param {!ScreenRect} rect1
   * @param {!ScreenRect} rect2
   * @return {boolean} True if the rects overlap.
   */
  overlaps: (rect1, rect2) => {
    return rect1.left < RectUtil.right(rect2) &&
        rect2.left < RectUtil.right(rect1) &&
        rect1.top < RectUtil.bottom(rect2) &&
        rect2.top < RectUtil.bottom(rect1);
  },

  /**
   * Finds the right edge of a rect.
   * @param {!ScreenRect} rect
   * @return {number}
   */
  right: rect => rect.left + rect.width,

  /*
   * @param {ScreenRect=} rect1
   * @param {ScreenRect=} rect2
   * @return {boolean}
   */
  sameRow: (rect1, rect2) => {
    if (!rect1 || !rect2) {
      return false;
    }
    const bottom1 = RectUtil.bottom(rect1);
    const middle2 = RectUtil.center(rect2).y;

    return rect1.top < middle2 && bottom1 > middle2;
  },

  /**
   * Returns a string representing the given rectangle.
   * @param {ScreenRect|undefined} rect
   * @return {string}
   */
  toString: rect => {
    let str = '';
    if (rect) {
      str = rect.left + ',' + rect.top + ' ';
      str += rect.width + 'x' + rect.height;
    }
    return str;
  },

  /**
   * Returns the union of the specified rectangles.
   * @param {!ScreenRect} rect1
   * @param {!ScreenRect} rect2
   * @return {!ScreenRect}
   */
  union: (rect1, rect2) => {
    const top = rect1.top < rect2.top ? rect1.top : rect2.top;
    const left = rect1.left < rect2.left ? rect1.left : rect2.left;

    const r1Bottom = RectUtil.bottom(rect1);
    const r2Bottom = RectUtil.bottom(rect2);
    const bottom = r1Bottom > r2Bottom ? r1Bottom : r2Bottom;

    const r1Right = RectUtil.right(rect1);
    const r2Right = RectUtil.right(rect2);
    const right = r1Right > r2Right ? r1Right : r2Right;

    const height = bottom - top;
    const width = right - left;

    return {top, left, width, height};
  },

  /**
   * Returns the union of all the rectangles specified.
   * @param {!Array<!ScreenRect>} rects
   * @return {!ScreenRect}
   */
  unionAll: rects => {
    if (rects.length < 1) {
      return RectUtil.ZERO_RECT;
    }

    let result = rects[0];
    for (let i = 1; i < rects.length; i++) {
      result = RectUtil.union(result, rects[i]);
    }
    return result;
  },
};
