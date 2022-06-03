// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
GEN_INCLUDE([
  '../../common/testing/assert_additions.js',
  '../../common/testing/common.js',
  '../../common/testing/callback_helper.js'
]);
// clang-format on

/**
 * Base test fixture for ChromeVox webui tests. Run in a Blink renderer.
 */
ChromeVoxWebUITestBase = class extends testing.Test {
  constructor() {
    super();
    if (this.isAsync) {
      this.callbackHelper_ = new CallbackHelper(this);
    }
  }

  /** @override */
  testGenCppIncludes() {
    GEN(`
  #include "content/public/test/browser_test.h"
      `);
  }

  /**
   * Loads some inlined html into the body of the current document, replacing
   * whatever was there previously.
   * @param {string} html The html to load as a string.
   */
  loadHtml(html) {
    while (document.head.firstChild) {
      document.head.removeChild(document.head.firstChild);
    }
    while (document.body.firstChild) {
      document.body.removeChild(document.body.firstChild);
    }
    this.appendHtml(html);
  }

  /**
   * Loads some inlined html into the current document, replacing
   * whatever was there previously. This version takes the html
   * encoded as a multiline string. `
   * <button>
   * `
   * OBSOLETE: prior to multiline string support in js.
   * Use a comment inside a function, so you can use it like this:
   *
   * this.loadDoc(function() {/*!
   *     <p>Html goes here</p>
   * * /});
   *
   * @param {Function} commentEncodedHtml The html to load, embedded as a
   *     comment inside an anonymous function - see example, above.
   */
  loadDoc(commentEncodedHtml) {
    const html =
        TestUtils.extractHtmlFromCommentEncodedString(commentEncodedHtml);
    this.loadHtml(html);
  }

  /**
   * Appends some inlined html into the current document, at the end of
   * the body element. Takes the html encoded as a comment inside a function,
   * so you can use it like this:
   *
   * this.appendDoc(function() {/*!
   *     <p>Html goes here</p>
   * * /});
   *
   * @param {Function} commentEncodedHtml The html to load, embedded as a
   *     comment inside an anonymous function - see example, above.
   */
  appendDoc(commentEncodedHtml) {
    const html =
        TestUtils.extractHtmlFromCommentEncodedString(commentEncodedHtml);
    this.appendHtml(html);
  }

  /**
   * Appends some inlined html into the current document, at the end of
   * the body element.
   * @param {string} html The html to load as a string.
   */
  appendHtml(html) {
    const div = document.createElement('div');
    div.innerHTML = html;
    const fragment = document.createDocumentFragment();
    while (div.firstChild) {
      fragment.appendChild(div.firstChild);
    }
    document.body.appendChild(fragment);
  }

  /**
   * Creates a callback that optionally calls {@code opt_callback} when
   * called.  If this method is called one or more times, then
   * {@code testDone()} will be called when all callbacks have been called.
   * @param {Function=} opt_callback Wrapped callback that will have its this
   *        reference bound to the test fixture.
   * @return {Function}
   */
  newCallback(opt_callback) {
    assertNotEquals(null, this.callbackHelper_);
    return this.callbackHelper_.wrap(opt_callback);
  }
};

// Due to limitations of the test framework, we need to place members directly
// on the prototype. The framework cannot actually instantiate the object during
// its first pass where it uses this file to generate C++ code.

/** @override */
ChromeVoxWebUITestBase.prototype.isAsync = false;

/**
 * @override
 * It doesn't make sense to run the accessibility audit on these tests,
 * since many of them are deliberately testing inaccessible html.
 */
ChromeVoxWebUITestBase.prototype.runAccessibilityChecks = false;

/** @override */
ChromeVoxWebUITestBase.prototype.browsePreload = DUMMY_URL;
