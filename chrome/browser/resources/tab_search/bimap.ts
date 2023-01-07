// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'BiMap' is an implementation of a bidirectional map. It
 * facilitates looking up for associated pairs in either direction.
 */

// TODO(romanarora): Investigate leveraging an existing data structures third
// party library or moving this class to a shareable location.

export class BiMap<K, V> {
  private map_: Map<K, V>;
  private inverseMap_: Map<V, K>;

  constructor() {
    this.map_ = new Map<K, V>();
    this.inverseMap_ = new Map<V, K>();
  }

  get(key: K): V|undefined {
    return this.map_.get(key);
  }

  invGet(key: V): K|undefined {
    return this.inverseMap_.get(key);
  }

  set(key: K, value: V) {
    this.map_.set(key, value);
    this.inverseMap_.set(value, key);
  }

  invSet(key: V, value: K) {
    this.inverseMap_.set(key, value);
    this.map_.set(value, key);
  }

  size(): number {
    return this.map_.size;
  }
}
