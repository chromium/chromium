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
    ];

    const options = {
      includeScore: true,
      ignoreLocation: true,
      includeMatches: true,
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

    assertDeepEquals(matchedRecords, fuzzySearch('gle', records, options));
    assertDeepEquals(records, fuzzySearch('', records, options));
    assertDeepEquals([], fuzzySearch('z', records, options));
  });

  test('Test the exact match ranking order.', () => {
    // Set threshold to 0.0 to assert an exact match search.
    const options = {
      threshold: 0.0,
    };

    // Initial pre-search item list.
    const records = [
      {
        title: 'Code Search',
        hostname: 'search.chromium.search',
      },
      {title: 'Marching band', hostname: 'en.marching.band.com'},
      {
        title: 'Chrome Desktop Architecture',
        hostname: 'drive.google.com',
      },
      {
        title: 'Arch Linux',
        hostname: 'www.archlinux.org',
      },
      {
        title: 'Arches National Park',
        hostname: 'www.nps.gov',
      },
      {
        title: 'Search Engine Land - Search Engines',
        hostname: 'searchengineland.com'
      },
    ];

    // Resuts for 'arch'.
    const archMatchedRecords = [
      {
        title: 'Arch Linux',
        hostname: 'www.archlinux.org',
        titleHighlightRanges: [ {start: 0, length: 4} ],
        hostnameHighlightRanges: [ {start: 4, length: 4} ],
      },
      {
        title: 'Arches National Park',
        hostname: 'www.nps.gov',
        titleHighlightRanges: [ {start: 0, length: 4} ],
      },
      {
        title: 'Chrome Desktop Architecture',
        hostname: 'drive.google.com',
        titleHighlightRanges: [ {start: 15, length: 4} ],
      },
      {
        title: 'Code Search',
        hostname: 'search.chromium.search',
        titleHighlightRanges: [ {start: 7, length: 4} ],
        hostnameHighlightRanges:
            [ {start: 2, length: 4}, {start: 18, length: 4} ],
      },
      {
        title: 'Marching band',
        hostname: 'en.marching.band.com',
        titleHighlightRanges: [ {start: 1, length: 4} ],
        hostnameHighlightRanges: [ {start: 4, length: 4} ]
      },
      {
        title: 'Search Engine Land - Search Engines',
        hostname: 'searchengineland.com',
        titleHighlightRanges:
            [ {start: 2, length: 4}, {start: 23, length: 4} ],
        hostnameHighlightRanges: [ {start: 2, length: 4} ]
      },
    ];

    // Results for 'search'.
    const searchMatchedRecords = [
      {
        title: 'Code Search',
        hostname: 'search.chromium.search',
        titleHighlightRanges: [ {start: 5, length: 6} ],
        hostnameHighlightRanges:
            [ {start: 0, length: 6}, {start: 16, length: 6} ],
      },
      {
        title: 'Search Engine Land - Search Engines',
        hostname: 'searchengineland.com',
        titleHighlightRanges:
            [ {start: 0, length: 6}, {start: 21, length: 6} ],
        hostnameHighlightRanges: [ {start: 0, length: 6} ]
      },
    ];

    // Empty search should return the full list.
    assertDeepEquals(records, fuzzySearch('', records, options));

    assertDeepEquals(archMatchedRecords, fuzzySearch('arch', records, options));
    assertDeepEquals(searchMatchedRecords,
                     fuzzySearch('search', records, options));

    // No matches should return an empty list.
    assertDeepEquals([], fuzzySearch('archh', records, options));
  });

  test('Test exact search with escaped characters.', () => {
    // Set threshold to 0.0 to assert an exact match search.
    const options = {
      threshold: 0.0,
    };

    // Initial pre-search item list.
    const records = [ {
      title: '\'beginning\\test\\end',
      hostname: 'beginning\\test\"end',
    } ];

    // Expected results for '\test'.
    const backslashMatchedRecords = [
      {
        title: '\'beginning\\test\\end',
        hostname: 'beginning\\test\"end',
        titleHighlightRanges: [ {start: 10, length: 5} ],
        hostnameHighlightRanges: [ {start: 9, length: 5} ]
      },
    ];

    // Expected results for '"end'.
    const quoteMatchedRecords = [
      {
        title: '\'beginning\\test\\end',
        hostname: 'beginning\\test\"end',
        hostnameHighlightRanges: [ {start: 14, length: 4} ],
      },
    ];

    assertDeepEquals(backslashMatchedRecords,
                     fuzzySearch('\\test', records, options));
    assertDeepEquals(quoteMatchedRecords,
                     fuzzySearch('\"end', records, options));
  });

});
