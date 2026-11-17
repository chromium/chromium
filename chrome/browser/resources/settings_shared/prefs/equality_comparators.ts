// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Checks whether two values are recursively equal. Only compares serializable
 * data (primitives, serializable arrays and serializable objects).
 * @param val1 Value to compare.
 * @param val2 Value to compare with val1.
 * @return Whether the values are recursively equal.
 */
export function deepEqual(val1: unknown, val2: unknown): boolean {
  if (val1 === val2) {
    return true;
  }

  if (Array.isArray(val1) || Array.isArray(val2)) {
    if (!Array.isArray(val1) || !Array.isArray(val2)) {
      return false;
    }
    return arraysEqual(val1, val2);
  }

  if (val1 instanceof Object && val2 instanceof Object) {
    return objectsEqual(
        val1 as Record<string, unknown>, val2 as Record<string, unknown>);
  }

  return false;
}

function arraysEqual(arr1: unknown[], arr2: unknown[]): boolean {
  if (arr1.length !== arr2.length) {
    return false;
  }

  for (let i = 0; i < arr1.length; i++) {
    if (!deepEqual(arr1[i], arr2[i])) {
      return false;
    }
  }

  return true;
}

function objectsEqual(
    obj1: Record<string, unknown>, obj2: Record<string, unknown>): boolean {
  const keys1 = Object.keys(obj1);
  const keys2 = Object.keys(obj2);
  if (keys1.length !== keys2.length) {
    return false;
  }

  for (const key of keys1) {
    if (!deepEqual(obj1[key], obj2[key])) {
      return false;
    }
  }

  return true;
}
