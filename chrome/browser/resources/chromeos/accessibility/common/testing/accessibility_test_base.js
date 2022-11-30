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
};
