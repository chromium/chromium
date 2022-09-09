// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js');

promise_test(async () => {
  assert_true(chromeos.windowManagement instanceof EventTarget);

  return new Promise(resolve => {
    chromeos.windowManagement.addEventListener('testevent', e => {
      assert_equals(e.target, chromeos.windowManagement);
      resolve();
    });
    chromeos.windowManagement.dispatchEvent(new Event('testevent'));
  });
});
