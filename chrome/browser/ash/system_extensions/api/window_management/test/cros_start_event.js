// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

// Add event listener here because the event might get triggered before the test
// runs.
const startEventPromise = new Promise(resolve => {
  chromeos.windowManagement.addEventListener('start', resolve);
});

promise_test(async (t) => {
  let event = await startEventPromise;

  assert_true(event instanceof Event);
  assert_equals(event.type, 'start');

  // The `start` event doesn't bubble up to `chromeos` or any other objects.
  assert_false(event.bubbles);

  // The extension starting can't be cancelled.
  assert_false(event.cancelable);
});
