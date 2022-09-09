// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

// Tests that Web App tabs are not returned as windows. This test is necessary
// because InstanceRegistry, which we use to get the list of windows, includes
// Web App tabs.
promise_test(async () => {
  // Before the test starts, we installed a Web App that opens in a tab
  // and launched it.
  let windows = await chromeos.windowManagement.getWindows();
  assert_equals(windows.length, 1);
});
