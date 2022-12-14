// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['testing/common_e2e_test_base.js']);

/** Test fixture for rect_util.js. */
AccessibilityExtensionRectUtilTest = class extends CommonE2ETestBase {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule('RectUtil', '/common/rect_util.js');
  }
};

AX_TEST_F('AccessibilityExtensionRectUtilTest', 'Adjacent', function() {
  const baseRect = {left: 10, top: 10, width: 10, height: 10};
  const adjacentRects = [
    {left: 0, top: 0, width: 10, height: 10},
    {left: 7, top: 0, width: 10, height: 10},
    {left: 10, top: 0, width: 10, height: 10},
    {left: 17, top: 0, width: 10, height: 10},
    {left: 20, top: 0, width: 10, height: 10},
    {left: 0, top: 7, width: 10, height: 10},
    {left: 20, top: 7, width: 10, height: 10},
    {left: 0, top: 10, width: 10, height: 10},
    {left: 20, top: 10, width: 10, height: 10},
    {left: 0, top: 17, width: 10, height: 10},
    {left: 20, top: 17, width: 10, height: 10},
    {left: 0, top: 20, width: 10, height: 10},
    {left: 7, top: 20, width: 10, height: 10},
    {left: 10, top: 20, width: 10, height: 10},
    {left: 17, top: 20, width: 10, height: 10},
    {left: 20, top: 20, width: 10, height: 10},
  ];
  const nonAdjacentRects = [
    {left: 5, top: 0, width: 5, height: 5},
    {left: 20, top: 0, width: 5, height: 5},
    {left: 12, top: 4, width: 5, height: 5},
    {left: 0, top: 5, width: 5, height: 5},
    {left: 25, top: 5, width: 5, height: 5},
    {left: 12, top: 6, width: 5, height: 5},
    {left: 4, top: 12, width: 5, height: 5},
    {left: 6, top: 12, width: 5, height: 5},
    {left: 12, top: 12, width: 5, height: 5},
    {left: 19, top: 12, width: 5, height: 5},
    {left: 21, top: 12, width: 5, height: 5},
    {left: 12, top: 19, width: 5, height: 5},
    {left: 0, top: 20, width: 5, height: 5},
    {left: 21, top: 20, width: 5, height: 5},
    {left: 12, top: 21, width: 5, height: 5},
    {left: 5, top: 25, width: 5, height: 5},
    {left: 20, top: 25, width: 5, height: 5},
  ];

  for (const rect of adjacentRects) {
    assertTrue(
        RectUtil.adjacent(baseRect, rect),
        RectUtil.toString(baseRect) + ' should be adjacent to ' +
            RectUtil.toString(rect));
    assertTrue(
        RectUtil.adjacent(rect, baseRect),
        RectUtil.toString(rect) + ' should be adjacent to ' +
            RectUtil.toString(baseRect));
  }

  for (const rect of nonAdjacentRects) {
    assertFalse(
        RectUtil.adjacent(baseRect, rect),
        RectUtil.toString(baseRect) + ' should not be adjacent to ' +
            RectUtil.toString(rect));
    assertFalse(
        RectUtil.adjacent(rect, baseRect),
        RectUtil.toString(rect) + ' should not be adjacent to ' +
            RectUtil.toString(baseRect));
  }
});

AX_TEST_F('AccessibilityExtensionRectUtilTest', 'Close', function() {
  const centerRect = {left: 10, top: 10, width: 10, height: 10};
  assertTrue(RectUtil.close(centerRect, centerRect, 0));

  // Adjacent to the left of centerRect.
  const leftRect = {left: 5, top: 10, width: 5, height: 5};
  assertTrue(RectUtil.close(centerRect, leftRect, 5));
  assertTrue(RectUtil.close(centerRect, leftRect, 0));
  assertTrue(RectUtil.close(leftRect, centerRect, 0));

  // 2dp away to the right of centerRect.
  const rightRect = {left: 22, top: 17, width: 5, height: 5};
  assertTrue(RectUtil.close(centerRect, rightRect, 5));
  assertTrue(RectUtil.close(rightRect, centerRect, 5));
  assertFalse(RectUtil.close(centerRect, rightRect, 0));
  assertFalse(RectUtil.close(rightRect, centerRect, 0));

  // 5dp above centerRect.
  const topRect = {left: 12, top: 0, width: 5, height: 5};
  assertTrue(RectUtil.close(centerRect, topRect, 5));
  assertTrue(RectUtil.close(topRect, centerRect, 5));
  assertFalse(RectUtil.close(centerRect, topRect, 4));
  assertFalse(RectUtil.close(topRect, centerRect, 4));

  // Adjacent below centerRect.
  const bottomRect = {left: 15, top: 20, width: 5, height: 5};
  assertTrue(RectUtil.close(centerRect, bottomRect, 0));
  assertTrue(RectUtil.close(bottomRect, centerRect, 0));

  // Touching at the top left corner of centerRect.
  const topLeftRect = {left: 5, top: 5, width: 5, height: 5};
  assertTrue(RectUtil.close(centerRect, topLeftRect, 0));
  assertTrue(RectUtil.close(topLeftRect, centerRect, 0));

  // 1dp off in each dimension from the bottom right corner of centerRect.
  const bottomRightRect = {left: 21, top: 21, width: 5, height: 5};
  assertTrue(RectUtil.close(centerRect, bottomRightRect, 1));
  assertTrue(RectUtil.close(bottomRightRect, centerRect, 1));
  assertFalse(RectUtil.close(centerRect, bottomRightRect, 0));
  assertFalse(RectUtil.close(bottomRightRect, centerRect, 0));

  // Overlapping horizontally, but far vertically.
  const verticallyFarRect = {left: 15, top: 100, width: 5, height: 5};
  assertFalse(RectUtil.close(centerRect, verticallyFarRect, 5));
  assertFalse(RectUtil.close(verticallyFarRect, centerRect, 5));

  // Overlapping vertically, but far horizontally.
  const horizontallyFarRect = {left: 100, top: 15, width: 5, height: 5};
  assertFalse(RectUtil.close(centerRect, horizontallyFarRect, 5));
  assertFalse(RectUtil.close(horizontallyFarRect, centerRect, 5));

  // Far away in both dimensions from centerRect.
  const farRect = {left: 100, top: 100, width: 1, height: 1};
  assertFalse(RectUtil.close(centerRect, farRect, 20));
  assertFalse(RectUtil.close(farRect, centerRect, 20));
});

AX_TEST_F('AccessibilityExtensionRectUtilTest', 'Equals', function() {
  const rect1 = {left: 0, top: 0, width: 10, height: 10};
  const rect2 = {left: 0, top: 0, width: 10, height: 10};
  const rect3 = {left: 1, top: 0, width: 10, height: 10};
  const rect4 = {left: 0, top: 1, width: 10, height: 10};
  const rect5 = {left: 0, top: 0, width: 11, height: 10};
  const rect6 = {left: 0, top: 0, width: 10, height: 11};

  assertTrue(RectUtil.equal(rect1, rect1), 'equal should be reflexive');
  assertTrue(RectUtil.equal(rect1, rect2), 'Rect1 and Rect2 should be equal');
  assertTrue(RectUtil.equal(rect2, rect1), 'equal should be symmetric');
  assertFalse(
      RectUtil.equal(rect1, rect3), 'rect1 and rect3 should not be equal');
  assertFalse(RectUtil.equal(rect3, rect1), 'equal should be symmetric');
  assertFalse(
      RectUtil.equal(rect1, rect4), 'rect1 and rect4 should not be equal');
  assertFalse(RectUtil.equal(rect4, rect1), 'equal should be symmetric');
  assertFalse(
      RectUtil.equal(rect1, rect5), 'rect1 and rect5 should not be equal');
  assertFalse(RectUtil.equal(rect5, rect1), 'equal should be symmetric');
  assertFalse(
      RectUtil.equal(rect1, rect6), 'rect1 and rect6 should not be equal');
  assertFalse(RectUtil.equal(rect6, rect1), 'equal should be symmetric');
});

AX_TEST_F('AccessibilityExtensionRectUtilTest', 'Center', function() {
  const rect1 = {left: 0, top: 0, width: 10, height: 10};
  const rect2 = {left: 10, top: 20, width: 10, height: 40};

  const center1 = RectUtil.center(rect1);
  assertEquals(5, center1.x, 'Center1 x should be 5');
  assertEquals(5, center1.y, 'Center1 y should be 5');

  const center2 = RectUtil.center(rect2);
  assertEquals(15, center2.x, 'Center2 x should be 15');
  assertEquals(40, center2.y, 'Center2 y should be 40');
});

AX_TEST_F('AccessibilityExtensionRectUtilTest', 'Union', function() {
  const rect1 = {left: 0, top: 0, width: 10, height: 10};
  const rect2 = {left: 4, top: 4, width: 2, height: 2};
  const rect3 = {left: 10, top: 20, width: 10, height: 40};
  const rect4 = {left: 0, top: 10, width: 10, height: 10};
  const rect5 = {left: 5, top: 5, width: 10, height: 10};

  // When one rect entirely contains the other, that rect is returned.
  const unionRect1Rect2 = RectUtil.union(rect1, rect2);
  assertTrue(
      RectUtil.equal(rect1, unionRect1Rect2),
      'Union of rect1 and rect2 should be rect1');

  const unionRect1Rect3 = RectUtil.union(rect1, rect3);
  let expected = {left: 0, top: 0, width: 20, height: 60};
  assertTrue(
      RectUtil.equal(expected, unionRect1Rect3),
      'Union of rect1 and rect3 does not match expected value');

  const unionRect1Rect4 = RectUtil.union(rect1, rect4);
  expected = {left: 0, top: 0, width: 10, height: 20};
  assertTrue(
      RectUtil.equal(expected, unionRect1Rect4),
      'Union of rect1 and rect4 does not match expected value');

  const unionRect1Rect5 = RectUtil.union(rect1, rect5);
  expected = {left: 0, top: 0, width: 15, height: 15};
  assertTrue(
      RectUtil.equal(expected, unionRect1Rect5),
      'Union of rect1 and rect5 does not match expected value');
});

AX_TEST_F('AccessibilityExtensionRectUtilTest', 'UnionAll', function() {
  const rect1 = {left: 0, top: 0, width: 10, height: 10};
  const rect2 = {left: 0, top: 10, width: 10, height: 10};
  const rect3 = {left: 10, top: 0, width: 10, height: 10};
  const rect4 = {left: 10, top: 10, width: 10, height: 10};
  const rect5 = {left: 0, top: 0, width: 100, height: 10};

  const union1 = RectUtil.unionAll([rect1, rect2, rect3, rect4]);
  let expected = {left: 0, top: 0, width: 20, height: 20};
  assertTrue(
      RectUtil.equal(expected, union1),
      'Union of rects 1-4 does not match expected value');

  const union2 = RectUtil.unionAll([rect1, rect2, rect3, rect4, rect5]);
  expected = {left: 0, top: 0, width: 100, height: 20};
  assertTrue(
      RectUtil.equal(expected, union2),
      'Union of rects 1-5 does not match expected value');
});

AX_TEST_F(
    'AccessibilityExtensionRectUtilTest', 'ExpandToFitWithPadding', function() {
      const padding = 5;
      let inner = {left: 100, top: 100, width: 100, height: 100};
      let outer = {left: 120, top: 120, width: 20, height: 20};
      let expected = {left: 95, top: 95, width: 110, height: 110};
      assertTrue(
          RectUtil.equal(
              expected, RectUtil.expandToFitWithPadding(padding, outer, inner)),
          'When outer is contained in inner, expandToFitWithPadding does not ' +
              'match expected value');

      inner = {left: 100, top: 100, width: 100, height: 100};
      outer = {left: 50, top: 50, width: 200, height: 200};
      assertTrue(
          RectUtil.equal(
              outer, RectUtil.expandToFitWithPadding(padding, outer, inner)),
          'When outer contains inner, expandToFitWithPadding should equal ' +
              'outer');

      inner = {left: 100, top: 100, width: 100, height: 100};
      outer = {left: 10, top: 10, width: 10, height: 10};
      expected = {left: 10, top: 10, width: 195, height: 195};
      assertTrue(
          RectUtil.equal(
              expected, RectUtil.expandToFitWithPadding(padding, outer, inner)),
          'When there is no overlap, expandToFitWithPadding does not match ' +
              'expected value');

      inner = {left: 100, top: 100, width: 100, height: 100};
      outer = {left: 120, top: 50, width: 200, height: 200};
      expected = {left: 95, top: 50, width: 225, height: 200};
      assertTrue(
          RectUtil.equal(
              expected, RectUtil.expandToFitWithPadding(padding, outer, inner)),
          'When there is some overlap, expandToFitWithPadding does not match ' +
              'expected value');

      inner = {left: 100, top: 100, width: 100, height: 100};
      outer = {left: 97, top: 95, width: 108, height: 110};
      expected = {left: 95, top: 95, width: 110, height: 110};
      assertTrue(
          RectUtil.equal(
              expected, RectUtil.expandToFitWithPadding(padding, outer, inner)),
          'When outer contains inner but without sufficient padding, ' +
              'expandToFitWithPadding does not match expected value');
    });

AX_TEST_F('AccessibilityExtensionRectUtilTest', 'Contains', function() {
  const outer = {left: 10, top: 10, width: 10, height: 10};
  assertTrue(RectUtil.contains(outer, outer), 'Rect should contain itself');

  let inner = {left: 10, top: 12, width: 10, height: 5};
  assertTrue(
      RectUtil.contains(outer, inner),
      'Rect should contain rect with same left/right bounds');
  inner = {left: 12, top: 10, width: 5, height: 10};
  assertTrue(
      RectUtil.contains(outer, inner),
      'Rect should contain rect with same top/bottom bounds');
  inner = {left: 12, top: 12, width: 5, height: 5};
  assertTrue(
      RectUtil.contains(outer, inner),
      'Rect should contain rect that is entirely within its bounds');

  inner = {left: 5, top: 12, width: 10, height: 5};
  assertFalse(
      RectUtil.contains(outer, inner),
      'Rect should not contain rect that extends past the left edge');
  inner = {left: 12, top: 8, width: 5, height: 10};
  assertFalse(
      RectUtil.contains(outer, inner),
      'Rect should not contain rect that extends past the top edge');
  inner = {left: 15, top: 5, width: 10, height: 20};
  assertFalse(
      RectUtil.contains(outer, inner),
      'Rect should not contain rect that extends past right edge and has ' +
          'larger height');
  inner = {left: 5, top: 15, width: 10, height: 10};
  assertFalse(
      RectUtil.contains(outer, inner),
      'Rect should not contain rect that extends below and left of it');
  inner = {left: 2, top: 12, width: 5, height: 5};
  assertFalse(
      RectUtil.contains(outer, inner),
      'Rect should not contain rect directly to its left');
  inner = {left: 12, top: 22, width: 5, height: 5};
  assertFalse(
      RectUtil.contains(outer, inner),
      'Rect should not contain rect directly below it');
  inner = {left: 22, top: 2, width: 5, height: 5};
  assertFalse(
      RectUtil.contains(outer, inner),
      'Rect should not contain rect above the the top-right corner');
  inner = {left: 12, top: 2, width: 5, height: 20};
  assertFalse(
      RectUtil.contains(outer, inner),
      'Rect should not contain rect that has a greater height');
  inner = {left: 2, top: 12, width: 20, height: 5};
  assertFalse(
      RectUtil.contains(outer, inner),
      'Rect should not contain rect that has a larger width');
});


AX_TEST_F('AccessibilityExtensionRectUtilTest', 'Difference', function() {
  const outer = {left: 10, top: 10, width: 10, height: 10};
  assertTrue(
      RectUtil.equal(RectUtil.ZERO_RECT, RectUtil.difference(outer, outer)),
      'Difference of rect with itself should the zero rect');

  let subtrahend = {left: 2, top: 2, width: 5, height: 5};
  assertTrue(
      RectUtil.equal(outer, RectUtil.difference(outer, subtrahend)),
      'Difference of non-overlapping rects should be the outer rect');

  subtrahend = {left: 5, top: 5, width: 20, height: 20};
  assertTrue(
      RectUtil.equal(
          RectUtil.ZERO_RECT, RectUtil.difference(outer, subtrahend)),
      'Difference where subtrahend contains outer should be the zero rect');

  subtrahend = {left: 12, top: 15, width: 6, height: 3};
  let expected = {left: 10, top: 10, width: 10, height: 5};
  assertTrue(
      RectUtil.equal(expected, RectUtil.difference(outer, subtrahend)),
      'Difference above should be largest');

  subtrahend = {left: 15, top: 8, width: 3, height: 10};
  expected = {left: 10, top: 10, width: 5, height: 10};
  assertTrue(
      RectUtil.equal(expected, RectUtil.difference(outer, subtrahend)),
      'Difference to the left should be the largest');

  subtrahend = {left: 5, top: 5, width: 13, height: 10};
  expected = {left: 10, top: 15, width: 10, height: 5};
  assertTrue(
      RectUtil.equal(expected, RectUtil.difference(outer, subtrahend)),
      'Difference below should be the largest');

  subtrahend = {left: 8, top: 8, width: 10, height: 15};
  expected = {left: 18, top: 10, width: 2, height: 10};
  assertTrue(
      RectUtil.equal(expected, RectUtil.difference(outer, subtrahend)),
      'Difference to the right should be the largest');
});

AX_TEST_F('AccessibilityExtensionRectUtilTest', 'Intersection', function() {
  const rect1 = {left: 10, top: 10, width: 10, height: 10};
  assertTrue(
      RectUtil.equal(rect1, RectUtil.intersection(rect1, rect1)),
      'Intersection of a rectangle with itself should be itself');

  let rect2 = {left: 12, top: 12, width: 5, height: 5};
  assertTrue(
      RectUtil.equal(rect2, RectUtil.intersection(rect1, rect2)),
      'When one rect contains another, intersection should be the inner rect');
  assertTrue(
      RectUtil.equal(rect2, RectUtil.intersection(rect2, rect1)),
      'Intersection should be symmetric');

  rect2 = {left: 5, top: 5, width: 20, height: 20};
  assertTrue(
      RectUtil.equal(rect1, RectUtil.intersection(rect1, rect2)),
      'When one rect contains another, intersection should be the inner rect');
  assertTrue(
      RectUtil.equal(rect1, RectUtil.intersection(rect2, rect1)),
      'Intersection should be symmetric');

  rect2 = {left: 30, top: 10, width: 10, height: 10};
  assertTrue(
      RectUtil.equal(RectUtil.ZERO_RECT, RectUtil.intersection(rect1, rect2)),
      'Intersection of non-overlapping rects should be zero rect');
  assertTrue(
      RectUtil.equal(RectUtil.ZERO_RECT, RectUtil.intersection(rect2, rect1)),
      'Intersection should be symmetric');

  rect2 = {left: 15, top: 10, width: 10, height: 10};
  let expected = {left: 15, top: 10, width: 5, height: 10};
  assertTrue(
      RectUtil.equal(expected, RectUtil.intersection(rect1, rect2)),
      'Side-by-side overlap is not computed correctly');
  assertTrue(
      RectUtil.equal(expected, RectUtil.intersection(rect2, rect1)),
      'Intersection should be symmetric');

  rect2 = {left: 15, top: 5, width: 10, height: 10};
  expected = {left: 15, top: 10, width: 5, height: 5};
  assertTrue(
      RectUtil.equal(expected, RectUtil.intersection(rect1, rect2)),
      'Top corner overlap is not computed correctly');
  assertTrue(
      RectUtil.equal(expected, RectUtil.intersection(rect2, rect1)),
      'Intersection should be symmetric');

  rect2 = {left: 5, top: 15, width: 20, height: 10};
  expected = {left: 10, top: 15, width: 10, height: 5};
  assertTrue(
      RectUtil.equal(expected, RectUtil.intersection(rect1, rect2)),
      'Bottom overlap is not computed correctly');
  assertTrue(
      RectUtil.equal(expected, RectUtil.intersection(rect2, rect1)),
      'Intersection should be symmetric');
});

AX_TEST_F('AccessibilityExtensionRectUtilTest', 'Overlaps', function() {
  var rect1 = {left: 0, top: 0, width: 100, height: 100};
  var rect2 = {left: 80, top: 0, width: 100, height: 20};
  var rect3 = {left: 0, top: 80, width: 20, height: 100};

  assertTrue(RectUtil.overlaps(rect1, rect1));
  assertTrue(RectUtil.overlaps(rect2, rect2));
  assertTrue(RectUtil.overlaps(rect3, rect3));
  assertTrue(RectUtil.overlaps(rect1, rect2));
  assertTrue(RectUtil.overlaps(rect1, rect3));
  assertFalse(RectUtil.overlaps(rect2, rect3));
});

AX_TEST_F('AccessibilityExtensionRectUtilTest', 'RectFromPoints', function() {
  var rect = {left: 10, top: 20, width: 50, height: 60};

  assertNotEquals(
      JSON.stringify(rect),
      JSON.stringify(RectUtil.rectFromPoints(0, 0, 10, 10)));
  assertEquals(
      JSON.stringify(rect),
      JSON.stringify(RectUtil.rectFromPoints(10, 20, 60, 80)));
  assertEquals(
      JSON.stringify(rect),
      JSON.stringify(RectUtil.rectFromPoints(60, 20, 10, 80)));
  assertEquals(
      JSON.stringify(rect),
      JSON.stringify(RectUtil.rectFromPoints(10, 80, 60, 20)));
  assertEquals(
      JSON.stringify(rect),
      JSON.stringify(RectUtil.rectFromPoints(60, 80, 10, 20)));
});
