// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRequiredElement} from 'chrome://resources/js/util.js';

/**
 * @fileoverview This JavaScript prints an error message, throws uncaught
 * exceptions, and otherwise does erroneous things for testing.
 *
 * Note that function names and error text are referenced in integration tests
 * (particularly tast tests); do not change them without updating the tests.
 */

/**
 * Logs an error when called. This is the "during page load" error.
 */
function logsErrorDuringPageLoadOuter() {
  logsErrorDuringPageLoadInner();
}

/**
 * Logs an error when called. This is a separate function from
 * logsErrorDuringPageLoadOuter() so that we get an interesting stack.
 */
function logsErrorDuringPageLoadInner() {
  console.error('WebUI JS Error: printing error on page load');
}

/**
 * Logs an error when called. This is the "from button click" error.
 */
function logsErrorFromButtonClickHandler() {
  logsErrorFromButtonClickInner();
}

/**
 * Logs an error when called. This is a separate function from
 * logsErrorFromButtonClickHandler() so that we get an interesting stack.
 */
function logsErrorFromButtonClickInner() {
  console.error('WebUI JS Error: printing error on button click');
}

/**
 * Throws an exception when called.
 */
function throwExceptionHandler() {
  throwExceptionInner();
}

/**
 * Throws an exception when called. This is a separate function from
 * throwExceptionHandler() so that we get an interesting stack.
 */
function throwExceptionInner() {
  throw new Error('WebUI JS Error: exception button clicked');
}

/**
 * Success callback for the promise. This should never be called.
 */
function promiseSuccessful() {
  console.error('WebUI JS Error: Promise success. This should never happen');
}

/**
 * Creates a promise which will be rejected and doesn't handle the rejection.
 */
function unhandledPromiseRejection() {
  const promise =
      Promise.reject('WebUI JS Error: The rejector always rejects!');
  promise.then(promiseSuccessful);
}

getRequiredElement('error-button').onclick = logsErrorFromButtonClickHandler;
getRequiredElement('exception-button').onclick = throwExceptionHandler;
getRequiredElement('promise-button').onclick = unhandledPromiseRejection;
logsErrorDuringPageLoadOuter();
