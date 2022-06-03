// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** A collection of helper functions when dealing with arrays. */
const ArrayUtil = {
  /**
   * @param {Array=} array1
   * @param {Array=} array2
   * @return {boolean}
   */
  contentsAreEqual: (array1, array2) => {
    if (!array1 || !array2 || array1.length !== array2.length) {
      return false;
    }
    for (let i = 0; i < array1.length; i++) {
      if (array1[i] !== array2[i]) {
        return false;
      }
    }
    return true;
  },
};
