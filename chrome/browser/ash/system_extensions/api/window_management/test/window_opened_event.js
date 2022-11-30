// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

promise_test(async (t) => {
  const eventWatcher =
      new EventWatcher(t, chromeos.windowManagement, ['windowopened']);

  const {windowId} = await testHelper.openBrowserWindow();

  const event = await eventWatcher.wait_for(['windowopened']);
  assert_equals(event.window.id, windowId);

  // The `windowopened` event doesn't bubble up to `chromeos` or any other
  // objects.
  assert_false(event.bubbles);

  // The `windowopened` event is not cancelable.
  assert_false(event.cancelable);
});

done();
