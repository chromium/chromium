// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

promise_test(async () => {
  // Check that window begins in non-fullscreen state.
  await assertWindowState("normal");

  // Minimized->Fullscreen->Maximized->Minimized
  await minimizeAndTest();
  await setFullscreenAndTest(true);
  await maximizeAndTest();
  await minimizeAndTest();

  // Reversing above: Minimized<-Fullscreen<-Maximized<-Minimized
  await maximizeAndTest();
  await setFullscreenAndTest(true);
  await minimizeAndTest();
});
