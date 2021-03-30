// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "chrome/browser/sync/test/integration/os_sync_test.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using preferences_helper::ChangeStringPref;
using preferences_helper::GetPrefs;
using testing::Eq;

class SingleClientOsPreferencesSyncTest : public OsSyncTest {
 public:
  SingleClientOsPreferencesSyncTest() : OsSyncTest(SINGLE_CLIENT) {}
  ~SingleClientOsPreferencesSyncTest() override = default;
};

IN_PROC_BROWSER_TEST_F(SingleClientOsPreferencesSyncTest, Sanity) {
  ASSERT_TRUE(chromeos::features::IsSplitSettingsSyncEnabled());
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Shelf alignment is a Chrome OS only preference.
  ChangeStringPref(/*profile_index=*/0, ash::prefs::kShelfAlignment,
                   ash::kShelfAlignmentRight);
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  EXPECT_THAT(GetPrefs(/*index=*/0)->GetString(ash::prefs::kShelfAlignment),
              Eq(ash::kShelfAlignmentRight));
}

IN_PROC_BROWSER_TEST_F(SingleClientOsPreferencesSyncTest,
                       DisablingOsSyncFeatureDisablesDataType) {
  ASSERT_TRUE(chromeos::features::IsSplitSettingsSyncEnabled());
  ASSERT_TRUE(SetupSync());
  syncer::SyncService* service = GetSyncService(0);
  syncer::SyncUserSettings* settings = service->GetUserSettings();

  EXPECT_TRUE(settings->IsOsSyncFeatureEnabled());
  EXPECT_TRUE(service->GetActiveDataTypes().Has(syncer::OS_PREFERENCES));

  settings->SetOsSyncFeatureEnabled(false);
  EXPECT_FALSE(settings->IsOsSyncFeatureEnabled());
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::OS_PREFERENCES));
}

}  // namespace
