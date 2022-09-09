// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

promise_test(async () => {
  // Check that the window begins in a non-fullscreen state.
  await assertWindowState('normal');

  // Try all transitions in the following order:
  // Maximize -> Minimize -> Fullscreen -> Maximize
  // Maximize -> Fullscreen -> Minimize -> Maximize

  // Maximize -> Minimize
  await maximizeAndTest();
  await minimizeAndTest();
  await restoreAndTest();

  // Minimize -> Fullscreen
  await minimizeAndTest();
  await setFullscreenAndTest(true);
  await restoreAndTest();

  // Fullscreen -> Maximize
  await setFullscreenAndTest(true);
  await maximizeAndTest();
  await restoreAndTest();

  // Maximize -> Fullscreen
  await maximizeAndTest();
  await setFullscreenAndTest(true);
  await restoreAndTest();

  // Fullscreen -> Minimize
  await setFullscreenAndTest(true);
  await minimizeAndTest();
  await restoreAndTest();

  // Minimize -> Maximize
  await minimizeAndTest();
  await maximizeAndTest();
  await restoreAndTest();
});
