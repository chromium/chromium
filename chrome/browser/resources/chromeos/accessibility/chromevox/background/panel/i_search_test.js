// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(
    ['//chrome/browser/resources/chromeos/accessibility/chromevox/testing/' +
     'chromevox_next_e2e_test_base.js']);

/**
 * Test fixture for ISearch.
 */
ChromeVoxISearchTest = class extends ChromeVoxNextE2ETest {
  constructor() {
    super();
    this.expect_ = [];
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule('ISearch', '/chromevox/background/panel/i_search.js');
  }

  overrideOutputFunctions(iSearch) {
    iSearch.onSearchReachedBoundary_ = (node) => {
      const expectCallback = this.expect_.shift();
      expectCallback({node, isBoundary: true});
    };
    iSearch.onSearchResultChanged_ = (node, start, end) => {
      const expectCallback = this.expect_.shift();
      expectCallback({node, start, end});
    };
  }

  expect(str) {
    return new Promise(resolve => {
      this.expect_.push(this.newCallback(args => {
        const node = args.node;
        let actual = node.name || node.role;
        if (args.start && args.end) {
          actual =
              'start=' + args.start + ' end=' + args.end + ' text=' + actual;
        }
        if (args.isBoundary) {
          actual = 'boundary=' + actual;
        }
        assertEquals(str, actual);
        resolve();
      }));
    });
  }

  get linksAndHeadingsDoc() {
    return `
      <p>start</p>
      <a href='#a'>Home</a>
      <a href='#b'>About US</a>
      <p>
        <h1>Latest Breaking News</h1>
        <a href='foo'>See more...</a>
      </p>
      <a href='#bar'>Questions?</a>
      <h2>Privacy Policy</h2>
      <p>end<span>of test</span></p>
    `;
  }
};

TEST_F('ChromeVoxISearchTest', 'Simple', async function() {
  const rootNode = await this.runWithLoadedTree(this.linksAndHeadingsDoc);
  const search = new ISearch(new cursors.Cursor(rootNode, 0));
  this.overrideOutputFunctions(search);

  // Simple forward search.
  search.search('US', 'forward');
  await this.expect('start=6 end=8 text=About US');

  search.search('start', 'backward');
  await this.expect('start');

  // Boundary (beginning).
  search.search('foo', 'backward');
  await this.expect('boundary=start');

  // Boundary (end).
  search.search('foo', 'forward');
  // Search "focus" doesn't move.
  await this.expect('boundary=start');

  // Mixed case substring.
  search.search('bReak', 'forward');
  await this.expect('start=7 end=12 text=Latest Breaking News');

  search.search('bReaki', 'forward');
  // Incremental search stays on the current node.
  await this.expect('start=7 end=13 text=Latest Breaking News');

  search.search('bReakio', 'forward');
  // No results for the search.
  await this.expect('boundary=Latest Breaking News');
});
