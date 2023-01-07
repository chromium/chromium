// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

promise_test(async () => {
  let [window] = await chromeos.windowManagement.getWindows();

  let x = window.screenLeft;
  let y = window.screenTop;
  x -= 20;
  y -= 20;

  await moveToAndTest(x, y);
});
