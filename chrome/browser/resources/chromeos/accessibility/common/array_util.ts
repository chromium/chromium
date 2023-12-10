// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** A collection of helper functions when dealing with arrays. */
export const ArrayUtil = {
  contentsAreEqual: <T>(array1?: T[], array2?: T[]): boolean => {
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
