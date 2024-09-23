// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The class provides a wrapper to an array that returns different object
 * reference on each operation, but share the same underlying buffer.
 *
 * This is useful when we want to mutably update array for performance
 * reason. Since lit updates checks object identity, this ensures that an
 * operation on the array would triggers a render.
 */
export class InteriorMutableArray<T> {
  constructor(private readonly arrayInternal: T[]) {}

  /**
   * Gets the underlying array.
   *
   * Note that the array shouldn't be manipulated directly, and all operation
   * on the array should be done by returning a new wrapper
   * `InteriorMutableArray`.
   */
  get array(): T[] {
    return this.arrayInternal;
  }

  get length(): number {
    return this.arrayInternal.length;
  }

  push(el: T): InteriorMutableArray<T> {
    this.arrayInternal.push(el);
    return new InteriorMutableArray(this.arrayInternal);
  }
}
