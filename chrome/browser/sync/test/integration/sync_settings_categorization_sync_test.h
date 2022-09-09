// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SETTINGS_CATEGORIZATION_SYNC_TEST_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SETTINGS_CATEGORIZATION_SYNC_TEST_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

// Test suite for Chrome OS sync. Enables the SyncSettingsCategorization
// feature. TODO(https://crbug.com/1227417): When SyncSettingsCategorization is
// on-by-default this class can be deleted.
class SyncSettingsCategorizationSyncTest : public SyncTest {
 public:
  explicit SyncSettingsCategorizationSyncTest(TestType type);
  ~SyncSettingsCategorizationSyncTest() override;

  SyncSettingsCategorizationSyncTest(
      const SyncSettingsCategorizationSyncTest&) = delete;
  SyncSettingsCategorizationSyncTest& operator=(
      const SyncSettingsCategorizationSyncTest&) = delete;

 private:
  // The names |scoped_feature_list_| and |feature_list_| are both used in
  // superclasses.
  base::test::ScopedFeatureList settings_feature_list_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SETTINGS_CATEGORIZATION_SYNC_TEST_H_
