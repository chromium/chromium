// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

promise_test(async (t) => {
  const eventWatcher =
      new EventWatcher(t, chromeos.windowManagement, ['acceleratordown']);

  // Test that "Shift + Arrow" doesn't trigger an event.
  await testHelper.simulatePressKey(
    ui.mojom.KeyboardCode.LEFT,
    ui.mojom.EVENT_FLAG_SHIFT_DOWN);

  // "Alt + Arrow left"
  {
    await testHelper.simulatePressKey(
      ui.mojom.KeyboardCode.LEFT,
      ui.mojom.EVENT_FLAG_ALT_DOWN);

    const event = await eventWatcher.wait_for(['acceleratordown']);
    assert_equals(event.type, 'acceleratordown');
    assert_equals(event.acceleratorName, 'Alt ArrowLeft');
  }

  // "Alt + Shift + Arrow left"
  {
    await testHelper.simulatePressKey(
      ui.mojom.KeyboardCode.LEFT,
      ui.mojom.EVENT_FLAG_SHIFT_DOWN | ui.mojom.EVENT_FLAG_ALT_DOWN);

    const event = await eventWatcher.wait_for(['acceleratordown']);
    assert_equals(event.type, 'acceleratordown');
    assert_equals(event.acceleratorName, 'Alt Shift ArrowLeft');
  }

  // Test Up works since it's sometimes remapped on ChromeOS.

  // "Alt + Arrow up"
  {
    await testHelper.simulatePressKey(
      ui.mojom.KeyboardCode.UP,
      ui.mojom.EVENT_FLAG_ALT_DOWN);

    const event = await eventWatcher.wait_for(['acceleratordown']);
    assert_equals(event.type, 'acceleratordown');
    assert_equals(event.acceleratorName, 'Alt ArrowUp');
  }

  // "Alt + Shift + Arrow up"
  {
    await testHelper.simulatePressKey(
      ui.mojom.KeyboardCode.UP,
      ui.mojom.EVENT_FLAG_SHIFT_DOWN | ui.mojom.EVENT_FLAG_ALT_DOWN);

    const event = await eventWatcher.wait_for(['acceleratordown']);
    assert_equals(event.type, 'acceleratordown');
    assert_equals(event.acceleratorName, 'Alt Shift ArrowUp');
  }
});
