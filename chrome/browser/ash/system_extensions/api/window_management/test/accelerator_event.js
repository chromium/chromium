// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

promise_test(async (t) => {
  const eventWatcher =
      new EventWatcher(t, chromeos.windowManagement, ['acceleratordown']);

  await testHelper.simulatePressKey(
      ui.mojom.KeyboardCode.A,
      ui.mojom.EVENT_FLAG_CONTROL_DOWN | ui.mojom.EVENT_FLAG_ALT_DOWN);

  const event = await eventWatcher.wait_for(['acceleratordown']);
  assert_true(event instanceof chromeos.CrosAcceleratorEvent);
  assert_equals(event.type, 'acceleratordown');
  assert_equals(event.acceleratorName, 'Control Alt KeyA');
  assert_false(event.repeat);

  // `Event.bubbles` indicates whether the event bubbles of the DOM tree.
  // AcceleratorEvent doesn't bubble so this should return false.
  assert_false(event.bubbles);

  // Accelerator events are considered to be consumed, i.e. the System Extension
  // always handles them, so there they are not cancelable.
  assert_false(event.cancelable);
});
