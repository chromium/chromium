// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

// When unsetting fullscreen from a previously minimized window,
// the window state should return to the last non-minimized state.
promise_test(async () => {
  await assertWindowState("normal");

  // Normal->Minimized->Fullscreen should unfullscreen to normal.
  await minimizeAndTest();
  await setFullscreenAndTest(true);
  await setFullscreenAndTest(false);
  await assertWindowState("normal");

  // Normal->Fullscreen->Minimized->Fullscreen should unfullscreen to normal.
  await setFullscreenAndTest(true);
  await minimizeAndTest();
  await setFullscreenAndTest(true);
  await setFullscreenAndTest(false);
  await assertWindowState("normal");

  // Maximized->Minimized->Fullscreen should unfullscreen to normal.
  await maximizeAndTest();
  await minimizeAndTest();
  await setFullscreenAndTest(true);
  await setFullscreenAndTest(false);
  await assertWindowState("maximized");

  // Maximized->Fullscreen->Minimized->Fullscreen should unfullscreen to normal.
  await setFullscreenAndTest(true);
  await minimizeAndTest();
  await setFullscreenAndTest(true);
  await setFullscreenAndTest(false);
  await assertWindowState("maximized");
});
