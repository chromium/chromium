// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview File containing test framework helper functions. */

/**
 * Similar to |TEST_F|. Generates a test for the given |testFixture|,
 * |testName|, and |testFunction|.
 * Used this variant when an |isAsync| fixture wants to temporarily mix in a
 * sync test.
 * @param {string} testFixture Fixture name.
 * @param {string} testName Test name.
 * @param {function} testFunction The test impl.
 */
function SYNC_TEST_F(testFixture, testName, testFunction) {
  TEST_F(testFixture, testName, function() {
    this.newCallback(testFunction)();
  });
}

/**
 * Helper to import a module, and expose it onto window.
 * @param {string|!Array<string>} toImport Names of the module exports to
 *     expose.
 * @param {string} path Path to the module js file.
 */
async function importModule(toImport, path) {
  const module = await import(path);
  let moduleNames;
  if (typeof (toImport) === 'string') {
    moduleNames = [toImport];
  } else if (typeof (toImport) === 'object') {
    moduleNames = toImport;
  } else {
    throw new Error('Invalid argument to importModule: ' + toImport);
  }

  for (const moduleName of moduleNames) {
    window[moduleName] = module[moduleName];
  }
}
