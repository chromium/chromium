// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

// This test is run as a part of
// CrosWindowManagementBrowserTest.CrosWindowWebAppTab. It tests for a
// regression regarding .getWindows() with a web app open in an unfocused tab
// (see b/244237286).
promise_test(async (t) => {
  // The C++ component of the test installs and opens a web app in a tab before
  // unfocusing the web app tab and running this test.
  let window = await chromeos.windowManagement.getWindows();
  assert_equals(window.length, 1);
});

done();
