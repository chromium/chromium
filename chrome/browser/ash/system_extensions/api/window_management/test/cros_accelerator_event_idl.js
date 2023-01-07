// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js');

promise_test(async () => {
  assert_true(chromeos.CrosAcceleratorEvent !== undefined, 'event');
  let accelerator_event = new chromeos.CrosAcceleratorEvent(
     'acceleratordown', {acceleratorName: 'close-window', repeat: false});
  assert_equals(accelerator_event.type, 'acceleratordown', 'event type');
  assert_equals(accelerator_event.acceleratorName, 'close-window');
  assert_false(accelerator_event.repeat);
  assert_false(accelerator_event.bubbles, 'bubbles');
  assert_false(accelerator_event.cancelable, 'cancelable');
});
