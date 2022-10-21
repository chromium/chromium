// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../../testing/chromevox_next_e2e_test_base.js']);

/**
 * Test fixture for automation_util.js.
 */
ChromeVoxLogStoreTest = class extends ChromeVoxNextE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    // Alphabetical based on file path.
    await importModule(
        'LogStore', '/chromevox/background/logging/log_store.js');
  }
};

AX_TEST_F('ChromeVoxLogStoreTest', 'ShortLogs', function() {
  const logStore = new LogStore();
  for (let i = 0; i < 100; i++) {
    logStore.writeTextLog('test' + i, 'speech');
  }

  const logs = logStore.getLogs();
  assertEquals(logs.length, 100);
  for (let i = 0; i < logs.length; i++) {
    assertEquals(logs[i].toString(), 'test' + i);
  }
});

AX_TEST_F('ChromeVoxLogStoreTest', 'LongLogs', function() {
  const logStore = new LogStore();
  for (let i = 0; i < LogStore.LOG_LIMIT + 500; i++) {
    logStore.writeTextLog('test' + i, 'speech');
  }

  const logs = logStore.getLogs();
  assertEquals(logs.length, LogStore.LOG_LIMIT);
  for (let i = 0; i < logs.length; i++) {
    assertEquals(logs[i].toString(), 'test' + (i + 500));
  }
});
