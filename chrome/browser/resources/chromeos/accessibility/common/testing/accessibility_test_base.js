// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Base test fixture for all tests of the accessibility component extensions.
 */
AccessibilityTestBase = class extends testing.Test {
  /** @override */
  setUp() {
    const runTest = this.deferRunTest(WhenTestDone.EXPECT);
    (async () => {
      await this.setUpDeferred();
      runTest();
    })();
  }

  /**
   * An async variant of setUp.
   * Derived classes should use importModules within this function to pull any
   * ES6 modules.
   */
  async setUpDeferred() {}

  /**
   * @param {Object} object The object the method is called on.
   * @param {string} method The name of the method being called.
   * @param {Function} callback The callback to be called once the method has
   *     finished. It receives the same parameters as the original method.
   * @param {function(): boolean} reset Whether the callback should be removed
   *     after is has been called. Defaults to an always false function.
   *     Receives the same parameters as the original method.
   */
  addCallbackPostMethod(object, method, callback, reset = () => false) {
    const original = object[method].bind(object);
    object[method] = async (...args) => {
      await original(...args);
      await callback(...args);
      if (await reset(...args)) {
        object[method] = original;
      }
    };
  }
};
