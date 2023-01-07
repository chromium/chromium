// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js');

promise_test(async () => {
  let windows = await chromeos.windowManagement.getWindows();
  assert_true(chromeos.CrosWindowEvent !== undefined, 'event');
  let window_event = new chromeos.CrosWindowEvent('windowclosed',
      {window: windows[0]});
  assert_equals(window_event.type, 'windowclosed', 'event type');
  assert_equals(window_event.window, windows[0], 'Window event has correct\
      window property');
  assert_false(window_event.bubbles, 'Window event should not bubble');
  assert_false(window_event.cancelable, 'Window event should not be\
      cancelable');
});
