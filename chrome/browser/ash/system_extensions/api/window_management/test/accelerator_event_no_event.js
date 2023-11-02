// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

promise_test(async (t) => {
  const eventWatcher =
      new EventWatcher(t, chromeos.windowManagement, ['acceleratordown']);

  // The following key presses shouldn't generate events.
  await testHelper.simulatePressKey(
      ui.mojom.KeyboardCode.A, ui.mojom.EVENT_FLAG_NONE);
  await testHelper.simulatePressKey(
      ui.mojom.KeyboardCode.A, ui.mojom.EVENT_FLAG_SHIFT_DOWN);
  await testHelper.simulatePressKey(
      ui.mojom.KeyboardCode.SHIFT, ui.mojom.EVENT_FLAG_SHIFT_DOWN);
  await testHelper.simulatePressKey(
      ui.mojom.KeyboardCode.CONTROL, ui.mojom.EVENT_FLAG_CONTROL_DOWN);

  // Should generate an event.
  await testHelper.simulatePressKey(
      ui.mojom.KeyboardCode.A,
      ui.mojom.EVENT_FLAG_CONTROL_DOWN | ui.mojom.EVENT_FLAG_ALT_DOWN);

  const events =
      await eventWatcher.wait_for(['acceleratordown'], {record: 'all'});
  assert_equals(events.length, 1);

  const event = events[0];
  assert_true(event instanceof chromeos.CrosAcceleratorEvent);
  assert_equals(event.type, 'acceleratordown');
  assert_equals(event.acceleratorName, 'Control Alt KeyA');
  assert_false(event.repeat);
  assert_false(event.bubbles);
  assert_false(event.cancelable);
});
