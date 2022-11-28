// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['testing/common_e2e_test_base.js']);

/** Test fixture for array_util.js. */
AccessibilityExtensionArrayUtilTest = class extends CommonE2ETestBase {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule('ArrayUtil', '/common/array_util.js');
  }
};

AX_TEST_F('AccessibilityExtensionArrayUtilTest', 'ContentsAreEqual', function() {
  const even1 = [2, 4, 6, 8];
  const even2 = [2, 4, 6, 8];
  const odd = [1, 3, 5, 7, 9];
  const powersOf2 = [2, 4, 8, 16];

  assertFalse(
      ArrayUtil.contentsAreEqual(even1, odd),
      'Arrays with different lengths should not be equal.');
  assertFalse(
      ArrayUtil.contentsAreEqual(even1, powersOf2),
      'Arrays with some common elements should not be equal.');
  assertTrue(
      ArrayUtil.contentsAreEqual(even1, even1),
      'Arrays should equal themselves.');
  assertTrue(
      ArrayUtil.contentsAreEqual(even1, even2),
      'Two different array objects with the same elements should be equal.');

  const obj = {};
  const arrayWithObj = [obj];
  const secondArrayWithObj = [obj];
  const arrayWithDifferentObj = [{}];

  assertNotEquals(
      arrayWithObj, secondArrayWithObj,
      'Different array instances with the same contents should not be ' +
          'equal with ===.');
  assertTrue(
      ArrayUtil.contentsAreEqual(arrayWithObj, secondArrayWithObj),
      'Different array instances with references to the same object ' +
          'instance should be equal with contentsAreEqual.');
  assertFalse(
      ArrayUtil.contentsAreEqual(arrayWithObj, arrayWithDifferentObj),
      'Arrays with different objects should not be equal (ArrayUtil.' +
          'contentsAreEqual uses shallow equality for the elements).');
});
