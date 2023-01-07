// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Script that imports all necessary scripts to run a test.
importScripts(
  'testharness.js',
  // Needed for using Mojo bindings.
  'mojo_bindings_lite.js',
  'system_extensions_test_runner.test-mojom-lite.js',
  // Registers callback that notify the browser of test harness events, e.g.
  // when tests finish running.
  'testharnessreport.js');

// Setup to populate `currentRun`. Runs in addition to `promise_setup` in
// testharnessreport.js
promise_setup(async () => {
  const {runName} = await testRunner.getCurrentRunName();
  globalThis.currentRun = runName;
});

// Creates a run with `runName`. The run will run when the C++ side calls
// WaitForRun("runName"). See
// //chrome/browser/ash/system_extensions/api/test_support/system_extensions_api_browsertest.h
function test_run(run, runName) {
  promise_test(async (t) => {
    if (runName !== globalThis.currentRun) {
      globalThis.skipTest(runName);
      return;
    }
    await run(t);
  }, runName);
}
