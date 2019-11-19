// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_OS_SYNC_TEST_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_OS_SYNC_TEST_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

// Test suite for Chrome OS sync. Enables the SplitSettingsSync feature and the
// OS sync enabled preference before running the test.
class OsSyncTest : public SyncTest {
 public:
  explicit OsSyncTest(TestType type);
  ~OsSyncTest() override;

  OsSyncTest(const OsSyncTest&) = delete;
  OsSyncTest& operator=(const OsSyncTest&) = delete;

  // SyncTest:
  bool SetupClients() override;

 private:
  // The names |scoped_feature_list_| and |feature_list_| are both used in
  // superclasses.
  base::test::ScopedFeatureList settings_feature_list_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_OS_SYNC_TEST_H_
