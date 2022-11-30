// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

// Regression test for b/229172753 where operations on web app windows,
// including System Web App windows, would cause a crash.
promise_test(async () => {
  const {windowId} = await testHelper.openSystemWebAppWindow();

  let windows = await chromeos.windowManagement.getWindows();
  assert_equals(windows.length, 2);

  let swa_window = windows.find(window => window.id === windowId);
  assert_not_equals(
      undefined, swa_window, `Could not find window with id: ${windowId}`);

  await swa_window.minimize();
  await swa_window.focus();
  await swa_window.maximize();
  await swa_window.setFullscreen(true);
  await swa_window.close();
});
