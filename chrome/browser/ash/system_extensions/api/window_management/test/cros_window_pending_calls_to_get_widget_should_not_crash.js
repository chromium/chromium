// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js');

promise_test(async () => {
  let [window] = await chromeos.windowManagement.getWindows();
  let fullscreenPromise = window.setFullscreen(true);
  for (let i = 0; i < 100; i++)
    window.setFullscreen(true);
  await fullscreenPromise;
});
