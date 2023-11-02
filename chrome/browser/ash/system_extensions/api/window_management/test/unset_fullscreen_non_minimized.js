// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

// When unsetting fullscreen from a previously normal or maximized window,
// the window state should return to its previous state.
promise_test(async () => {
  await assertWindowState("normal");

  await setFullscreenAndTest(true);
  await setFullscreenAndTest(false);
  await assertWindowState("normal");

  await maximizeAndTest();
  await setFullscreenAndTest(true);
  await setFullscreenAndTest(false);
  await assertWindowState("maximized");
});
