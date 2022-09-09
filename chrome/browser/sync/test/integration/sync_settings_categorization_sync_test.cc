// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_settings_categorization_sync_test.h"

#include "ash/constants/ash_features.h"

SyncSettingsCategorizationSyncTest::SyncSettingsCategorizationSyncTest(
    TestType type)
    : SyncTest(type) {
  settings_feature_list_.InitAndEnableFeature(
      ash::features::kSyncSettingsCategorization);
}

SyncSettingsCategorizationSyncTest::~SyncSettingsCategorizationSyncTest() =
    default;
