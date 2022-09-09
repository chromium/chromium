// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

promise_test(async () => {
  const [window] = await chromeos.windowManagement.getWindows();

  const {minimumSize} = await testHelper.getMinimumSize(window.id);

  const targetWidth = minimumSize.width - 20;
  const targetHeight = minimumSize.height - 20;

  const deltaWidth = targetWidth - window.width;
  const deltaHeight = targetHeight - window.height;

  await resizeByAndTest(deltaWidth, deltaHeight);
});
