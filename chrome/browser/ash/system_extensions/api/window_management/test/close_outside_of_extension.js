// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

promise_test(async (t) => {
  const {windowId} = await testHelper.openBrowserWindow();

  const eventWatcher =
      new EventWatcher(t, chromeos.windowManagement, ['windowclosed']);

  await testHelper.closeBrowserWindow(windowId);

  const event = await eventWatcher.wait_for(['windowclosed']);
  assert_equals(event.window.id, windowId);

  const windows = await chromeos.windowManagement.getWindows();
  assert_equals(windows.length, 1);
});
