// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../../testing/chromevox_next_e2e_test_base.js']);

/**
 * Test fixture for automation_util.js.
 * @constructor
 * @extends {ChromeVoxE2ETestBase}
 */
function ChromeVoxLogStoreTest() {
  ChromeVoxNextE2ETest.call(this);
}

ChromeVoxLogStoreTest.prototype = {
  __proto__: ChromeVoxNextE2ETest.prototype,
};

SYNC_TEST_F('ChromeVoxLogStoreTest', 'ShortLogs', function() {
  var logStore = new LogStore();
  for (var i = 0; i < 100; i++)
    logStore.writeTextLog('test' + i, 'speech');

  var logs = logStore.getLogs();
  assertEquals(logs.length, 100);
  for (var i = 0; i < logs.length; i++)
    assertEquals(logs[i].toString(), 'test' + i);
});

SYNC_TEST_F('ChromeVoxLogStoreTest', 'LongLogs', function() {
  var logStore = new LogStore();
  for (var i = 0; i < LogStore.LOG_LIMIT + 500; i++)
    logStore.writeTextLog('test' + i, 'speech');

  var logs = logStore.getLogs();
  assertEquals(logs.length, LogStore.LOG_LIMIT);
  for (var i = 0; i < logs.length; i++)
    assertEquals(logs[i].toString(), 'test' + (i + 500));
});
