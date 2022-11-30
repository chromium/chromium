// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

promise_test(async () => {
  await testHelper.setDisplays('0+0-1280x720,1280+600-1920x1080');
  const kShelfHeight =
      (await testHelper.getShelfHeight()).shelfHeight;

  let screens = await chromeos.windowManagement.getScreens();
  assert_equals(screens.length, 2);

  assert_equals(screens[0].width, 1280);
  assert_equals(screens[0].availWidth, 1280);
  assert_equals(screens[0].height, 720);
  assert_equals(screens[0].availHeight, 720 - kShelfHeight);
  assert_equals(screens[0].left, 0);
  assert_equals(screens[0].top, 0);

  assert_equals(screens[1].width, 1920);
  assert_equals(screens[1].availWidth, 1920);
  assert_equals(screens[1].height, 1080);
  assert_equals(screens[1].availHeight, 1080 - kShelfHeight);
  assert_equals(screens[1].left, 1280);

  // TODO(b/236793342): Uncomment when DisplayManagerTestApi::UpdateDisplay
  // correctly updates y position of display.
  // assert_equals(screens[1].top, 600);
});
