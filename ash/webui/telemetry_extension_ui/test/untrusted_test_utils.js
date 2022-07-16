// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Utilities function(s) used in untrusted_browsertest.js
 */

/**
 * Utility function to verify that promise throws an exception with the
 * expected error message.
 * @param { Promise<*> } promise
 * @param { !string } expectedErrorMessage
 */
async function verifyErrorMessage(promise, expectedErrorMessage) {
  let errorMessage = null;
  try {
    await promise;
  } catch (error) {
    errorMessage = error.message;
  }
  assertEquals(expectedErrorMessage, errorMessage);
}

/**
 * Utility function representing a callback that should never be called.
 */
function shouldNeverBeCalledCallback() {
  throw new Error('This callback should never be called.');
}
