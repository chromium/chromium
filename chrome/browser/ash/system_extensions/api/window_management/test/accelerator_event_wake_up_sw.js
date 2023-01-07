// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

// Nothing to do in the first run.
test_run(() => {}, 'First run');

const acceleratorDownPromise = new Promise(resolve => {
  chromeos.windowManagement.addEventListener('acceleratordown', resolve);
});

test_run(async (t) => {
  let event = await acceleratorDownPromise;
  // Test some basic properties to make sure it's the event we expect.
  assert_equals(event.type, 'acceleratordown');
  assert_equals(event.acceleratorName, 'Control Alt KeyA');
}, 'Test event properties');
