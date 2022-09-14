// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

promise_test(async (t) => {
  const eventWatcher =
      new EventWatcher(t, chromeos.windowManagement, ['acceleratordown']);

  await testHelper.simulatePressKey(
      ui.mojom.KeyboardCode.A,
      ui.mojom.EVENT_FLAG_SHIFT_DOWN | ui.mojom.EVENT_FLAG_ALT_DOWN);

  const event = await eventWatcher.wait_for(['acceleratordown']);
  assert_true(event instanceof chromeos.CrosAcceleratorEvent);
  assert_equals(event.type, 'acceleratordown');
  assert_equals(event.acceleratorName, 'Alt Shift KeyA');
  assert_false(event.repeat);
  assert_false(event.bubbles);
  assert_false(event.cancelable);
});
