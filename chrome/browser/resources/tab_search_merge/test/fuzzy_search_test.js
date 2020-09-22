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
        hostname: 'www.opengl.org',
      },
      {
        title: 'Google',
        hostname: 'www.google.com',
      },
    ];

    const matchedRecords = [
      {
        title: 'Google',
        hostname: 'www.google.com',
        titleHighlightRanges: [{start: 0, length: 1}, {start: 3, length: 3}],
        hostnameHighlightRanges: [{start: 4, length: 1}, {start: 7, length: 3}]
      },
      {
        title: 'OpenGL',
        hostname: 'www.opengl.org',
        titleHighlightRanges: [{start: 2, length: 1}, {start: 4, length: 2}],
        hostnameHighlightRanges: [
          {start: 6, length: 1}, {start: 8, length: 2}, {start: 13, length: 1}
        ],
      },
    ]
    assertDeepEquals(matchedRecords, fuzzySearch('gle', records));
    assertDeepEquals(records, fuzzySearch('', records));
    assertDeepEquals([], fuzzySearch('z', records));
  });
});
