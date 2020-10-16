// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This JavaScript throws an unhandled exception on page load, and
 * then another 3 seconds later.
 *
 * Note that function names and error text are referenced in integration tests
 * (particularly tast tests); do not change them without updating the tests.
 */

/**
 * Throws an exception when called. This throws the "after 3 seconds" exception.
 */
function throwExceptionAfterTimeoutFunction() {
  throwExceptionAfterTimeoutInner();
}

/**
 * Throws an exception when called. This is a separate function from
 * throwExceptionAfterTimeoutFunction() so that we get an interesting stack.
 */
function throwExceptionAfterTimeoutInner() {
  throw new Error('WebUI JS Exception: timeout expired');
}

/**
 * Throws an exception when called. This throws the "during page load"
 * exception.
 */
function throwExceptionDuringPageLoadFunction() {
  throwExceptionDuringPageLoadInner();
}

/**
 * Throws an exception when called. This is a separate function from
 * throwExceptionDuringPageLoadFunction() so that we get an interesting stack.
 */
function throwExceptionDuringPageLoadInner() {
  throw new Error('WebUI JS Exception: exception on page load');
}

setTimeout(throwExceptionAfterTimeoutFunction, 3000);
throwExceptionDuringPageLoadFunction();
