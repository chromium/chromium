// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_settings_categorization_sync_test.h"

#include "ash/constants/ash_features.h"

SyncSettingsCategorizationSyncTest::SyncSettingsCategorizationSyncTest(
    TestType type)
    : SyncTest(type) {
  // TODO(https://crbug.com/1227417): Remove SplitSettingsSync after migrating
  // the affected tests.
  settings_feature_list_.InitWithFeatures(
      {
          ash::features::kSyncSettingsCategorization,
          ash::features::kSplitSettingsSync,
      },
      {});
}

SyncSettingsCategorizationSyncTest::~SyncSettingsCategorizationSyncTest() =
    default;
