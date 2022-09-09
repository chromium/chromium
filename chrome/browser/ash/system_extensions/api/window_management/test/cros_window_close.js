// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

promise_test(async (t) => {
  // Open a browser window that takes focus.
  const {windowId} = await testHelper.openBrowserWindow();

  let windows = await chromeos.windowManagement.getWindows();
  assert_equals(windows.length, 2);

  let windowToClose = windows.find(window => window.id === windowId);
  assert_not_equals(undefined, windowToClose,
      `Could not find window with id: ${windowId};`);

  const eventWatcher =
    new EventWatcher(t, chromeos.windowManagement, ['windowclosed']);

  windowToClose.close();

  const event = await eventWatcher.wait_for(['windowclosed']);
  assert_equals(event.window.id, windowId);

  windows = await chromeos.windowManagement.getWindows();
  assert_equals(windows.length, 1);
});
