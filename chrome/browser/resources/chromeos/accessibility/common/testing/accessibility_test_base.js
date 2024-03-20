// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Base test fixture for all tests of the accessibility component extensions.
 */
AccessibilityTestBase = class extends testing.Test {
  constructor() {
    super();
    // Copy all exports onto the globalThis object.
    Object.assign(globalThis, TestImportManager.getImports());
  }

  /** @override */
  setUp() {
    const runTest = this.deferRunTest(WhenTestDone.EXPECT);
    (async () => {
      await this.setUpDeferred();
      runTest();
    })();
  }

  /** An async variant of setUp. */
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
      if (reset(...args)) {
        object[method] = original;
      }
      await original(...args);
      await callback(...args);
    };
  }

  /**
   * Begins listening for a specified method to be called.
   * @param {Object} object The object the method is called on.
   * @param {string} method The name of the method being called.
   * @return {function()} A callback that will assert the method was actually
   *    called.
   */
  prepareToExpectMethodCall(object, method) {
    let methodCalled = false;
    this.addCallbackPostMethod(
        object, method, () => methodCalled = true, () => true);
    return () => assertTrue(methodCalled, `Expected ${method}() to be called`);
  }

  /**
   * Begins listening for a specified method to ensure it isn't called.
   * @param {Object} object The object the method would be called on.
   * @param {string} method the name of the method that would be called.
   * @return {function()} A callback that verifies the method was not actually
   *    called, and resets the original method.
   */
  prepareToExpectMethodNotCalled(object, method) {
    let methodCalled = false;
    const originalMethod = object[method];
    this.addCallbackPostMethod(object, method, () => {
      methodCalled = true;
    });
    return () => {
      assertFalse(methodCalled);
      object[method] = originalMethod;
    };
  }

  /**
   * Allows all pending functions to be performed before preceding. Similar to
   * yield in other languages.
   * @return {!Promise}
   */
  waitForPendingMethods() {
    return new Promise(setTimeout);
  }
};
