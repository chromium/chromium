// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_consent_optional_sync_test.h"

#include "ash/constants/ash_features.h"

SyncConsentOptionalSyncTest::SyncConsentOptionalSyncTest(TestType type)
    : SyncTest(type) {
  // SyncSettingsCategorization is required for SyncConsentOptional.
  settings_feature_list_.InitWithFeatures(
      {
          ash::features::kSyncSettingsCategorization,
          ash::features::kSyncConsentOptional,
      },
      {});
}

SyncConsentOptionalSyncTest::~SyncConsentOptionalSyncTest() = default;
