// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/os_sync_test.h"

#include "ash/constants/ash_features.h"

OsSyncTest::OsSyncTest(TestType type) : SyncTest(type) {
  settings_feature_list_.InitAndEnableFeature(
      chromeos::features::kSplitSettingsSync);
}

OsSyncTest::~OsSyncTest() = default;
