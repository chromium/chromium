// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

promise_test(async () => {
  // Check that the window begins in a non-fullscreen state.
  await assertWindowState('normal');

  // Test that the state stays as `normal` when calling `restore`
  // on a window with `normal` state.
  await restoreAndTest();

  // Test that a minimized window changes to `normal` when calling
  // restore.
  await minimizeAndTest();
  await restoreAndTest();

  // Test that a maximized window changes to `normal` when calling
  // restore.
  await maximizeAndTest();
  await restoreAndTest();

  // Test that a fullscreened window changes to `normal` when calling
  // restore.
  await setFullscreenAndTest(true);
  await restoreAndTest();

  // Test that the state is still `normal` after calling `restore` on a
  // window that had a non-normal state previously.
  await restoreAndTest();
  await restoreAndTest();
});
