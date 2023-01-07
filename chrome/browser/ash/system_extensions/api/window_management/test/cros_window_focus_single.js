// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

// Checks that focusing a non-visible unfocused window correctly sets focus.
promise_test(async () => {
  await assertWindowState("normal");
  {
    let [window] = await chromeos.windowManagement.getWindows();
    assert_true(window.isFocused);
  }

  await minimizeAndTest();
  {
    let [window] = await chromeos.windowManagement.getWindows();
    assert_false(window.isFocused);
  }

  await focusAndTest();
});
