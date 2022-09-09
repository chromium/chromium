// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

promise_test(async () => {
  await assertWindowState("normal");

  await maximizeAndTest();

  // Repeating maximize should not change any properties.
  await maximizeAndTest();
});
