// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/os_sync_test.h"

#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

OsSyncTest::OsSyncTest(TestType type) : SyncTest(type) {
  settings_feature_list_.InitAndEnableFeature(
      chromeos::features::kSplitSettingsSync);
}

OsSyncTest::~OsSyncTest() = default;

bool OsSyncTest::SetupClients() {
  if (!SyncTest::SetupClients())
    return false;
  // Enable the OS sync feature for all profiles.
  for (Profile* profile : GetAllProfiles()) {
    PrefService* prefs = profile->GetPrefs();
    EXPECT_FALSE(prefs->GetBoolean(syncer::prefs::kOsSyncFeatureEnabled));
    prefs->SetBoolean(syncer::prefs::kOsSyncFeatureEnabled, true);
  }
  return true;
}
