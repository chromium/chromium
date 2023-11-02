// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview @externs
 * Scrappy externs file to support browsertest.js compilation in media_app_ui.
 * These functions are added to the global `this` object using code in
 * test_api.js that goes like:
 *
 * (function(exports) {
 *   // Lots more.
 *   exports.TEST_F = TEST_F;
 * })(this);
 *
 * Closure doesn't really know what to do about that. Test fixtures based on
 * mocha consume the assertFoo methods via a separate JS module (chai_assert.js)
 * but we need to inject our tests into a sandboxed iframe with a tight CSP, and
 * that may be incompatible with modules without more complexities.
 * See also https://crbug.com/1000989#c22 and b/160274783.
 */

function GEN(s) {}
function TEST_F(fixture, testCase, Function) {}
function GUEST_TEST(testCase, Function) {}
const testing = {
  Test: class {
    get browsePreload() {}
    get testGenPreamble() {}
    get extraLibraries() {}
    get isAsync() {}
    get featureList() {}
    get typedefCppFixture() {}
    setUp() {}
  },
};
function testDone() {}
function assertEquals(expected, actual, message = undefined) {}
function assertGE(lhs, rhs) {}
function assertNotEquals(lhs, rhs) {}
function assertDeepEquals(lhs, rhs) {}
