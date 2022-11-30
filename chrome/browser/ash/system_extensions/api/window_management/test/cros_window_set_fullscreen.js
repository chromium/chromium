// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

promise_test(async () => {
  // Check that the window begins in a non-fullscreen state.
  await assertWindowState("normal");

  // Check that window can be fullscreened and repeating maintains fullscreen.
  await setFullscreenAndTest(true);
  await setFullscreenAndTest(true);
});
