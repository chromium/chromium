// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js');

promise_test(async () => {
  let returnedWindows = await chromeos.windowManagement.getWindows();
  assert_array_equals(chromeos.windowManagement.windows, returnedWindows);

  let windows = chromeos.windowManagement.windows;
  await chromeos.windowManagement.getWindows();

  // TODO(b/232866765): Change to assert_array_equals() once we update the
  // cache.
  windows.forEach((window, index) => {
    assert_not_equals(window, chromeos.windowManagement.windows[index]);
  });
});
