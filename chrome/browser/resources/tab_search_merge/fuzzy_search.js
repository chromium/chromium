// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Fuse from './fuse.js';

const OPTIONS = {
  includeScore: true,
  ignoreLocation: true,
  keys: [
    {
      name: 'title',
      weight: 2,
    },
    {
      name: 'hostname',
      weight: 1,
    }
  ]
};

/**
 * @param {string} input
 * @param {!Array<tabSearch.mojom.Tab>} records
 * @return {!Array<tabSearch.mojom.Tab>}
 */
export function fuzzySearch(input, records) {
  if (input.length === 0) {
    return records;
  }
  return new Fuse(records, OPTIONS).search(input).map(_ => _.item);
}
