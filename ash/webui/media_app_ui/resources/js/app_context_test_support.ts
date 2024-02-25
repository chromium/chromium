// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Support code running in the released app to facilitate
 * testing. This file is present in the real app (not just tests), so it should
 * only be used for coverage of tests cases that can not be injected by the
 * test framework.
 */

// Event handlers for tests in MediaAppIntegrationTest for testing crash report
// integration. These can't be injected because errors on lines of injected code
// do not go to the error handlers installed on window in the real app context.
window.addEventListener('simulate-type-error-for-test', event => {
  (event as any).notAFunction();
});

window.addEventListener('simulate-unhandled-rejection-for-test', _event => {
  new Promise(_resolve => {
    const error = new Error('fake_throw');
    error.name = 'FakeErrorName';
    throw error;
  });
});

window.addEventListener(
    'simulate-unhandled-rejection-with-dom-exception-for-test', _event => {
      new Promise(_resolve => {
        throw new DOMException('Not a file.', 'NotAFile');
      });
    });
