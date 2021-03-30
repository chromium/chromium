// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'BiMap' is an implementation of a bidirectional map. It
 * facilitates looking up for associated pairs in either direction.
 */

// TODO(romanarora): Investigate leveraging an existing data structures third
// party library or moving this class to a shareable location.

/**
 * @template K, V
 */
export class BiMap {
  /**
   * @param {!Object=} map
   */
  constructor(map) {
    /**
     * @private {!Map<K, V>}
     */
    this.map_ = new Map();

    /**
     * @private {!Map<V, K>}
     */
    this.inverseMap_ = new Map();

    for (const key in map) {
      const value = map[key];
      this.map_.set(key, value);
      this.inverseMap_.set(value, key);
    }
  }

  /**
   * @param {K} key
   * @return {V}
   */
  get(key) {
    return this.map_.get(key);
  }

  /**
   * @param {V} key
   * @return {K}
   */
  invGet(key) {
    return this.inverseMap_.get(key);
  }

  /**
   * @param {K} key
   * @param {V} value
   */
  set(key, value) {
    this.map_.set(key, value);
    this.inverseMap_.set(value, key);
  }

  /**
   * @param {V} key
   * @param {K} value
   */
  invSet(key, value) {
    this.inverseMap_.set(key, value);
    this.map_.set(value, key);
  }

  /** @return {number} */
  size() {
    return this.map_.size;
  }
}
