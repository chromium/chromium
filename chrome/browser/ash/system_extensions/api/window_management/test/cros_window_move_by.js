// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

promise_test(async () => {
  let [window] = await chromeos.windowManagement.getWindows();

  await moveByAndTest(-20, -20);

  // Check that calling twice continues to move the window.
  await moveByAndTest(10, 10);
});
