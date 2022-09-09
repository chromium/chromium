// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE([
  '../../common/testing/accessibility_test_base.js',
  '../../common/testing/assert_additions.js',
  '../../common/testing/common.js',
  '../../common/testing/callback_helper.js',
]);

/**
 * Base test fixture for ChromeVox webui tests. Run in a Blink renderer.
 */
ChromeVoxWebUITestBase = class extends AccessibilityTestBase {
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

/** @override */
ChromeVoxWebUITestBase.prototype.browsePreload = DUMMY_URL;
