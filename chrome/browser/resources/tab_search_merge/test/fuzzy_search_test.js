// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fuzzySearch} from 'chrome://tab-search/fuzzy_search.js'

import {assertDeepEquals, assertEquals} from '../../chai_assert.js';

suite('FuzzySearchTest', () => {
  test('fuzzySearch', () => {
    const records = [
      {
        title: 'OpenGL',
        hostname: 'https://www.opengl.org',
      },
      {
        title: 'Google',
        hostname: 'https://www.google.com',
      },
    ];
    assertDeepEquals(
        [
          records[1],
          records[0],
        ],
        fuzzySearch('gle', records));
    assertDeepEquals(records, fuzzySearch('', records));
    assertDeepEquals([], fuzzySearch('z', records));
  });
});
