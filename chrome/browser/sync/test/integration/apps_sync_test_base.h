// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_APPS_SYNC_TEST_BASE_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_APPS_SYNC_TEST_BASE_H_

#include "chrome/browser/sync/test/integration/sync_test.h"

class AppsSyncTestBase : public SyncTest {
 public:
  explicit AppsSyncTestBase(TestType test_type);
  ~AppsSyncTestBase() override;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_APPS_SYNC_TEST_BASE_H_
