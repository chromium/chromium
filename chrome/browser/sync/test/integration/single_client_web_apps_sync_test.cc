// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/chromeos_buildflags.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/sync/test/integration/os_sync_test.h"
#include "chromeos/constants/chromeos_features.h"
#endif

using syncer::UserSelectableType;
using syncer::UserSelectableTypeSet;

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)

// These tests test the new Web Apps system with next generation sync.
//
// Chrome OS syncs apps as an OS type.
class SingleClientWebAppsOsSyncTest : public OsSyncTest {
 public:
  SingleClientWebAppsOsSyncTest() : OsSyncTest(SINGLE_CLIENT) {}
  ~SingleClientWebAppsOsSyncTest() override = default;
};

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsOsSyncTest,
                       DisablingOsSyncFeatureDisablesDataType) {
  ASSERT_TRUE(chromeos::features::IsSplitSettingsSyncEnabled());
  ASSERT_TRUE(SetupSync());
  syncer::ProfileSyncService* service = GetSyncService(0);
  syncer::SyncUserSettings* settings = service->GetUserSettings();

  EXPECT_TRUE(settings->IsOsSyncFeatureEnabled());
  EXPECT_TRUE(service->GetActiveDataTypes().Has(syncer::WEB_APPS));

  settings->SetOsSyncFeatureEnabled(false);
  EXPECT_FALSE(settings->IsOsSyncFeatureEnabled());
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::WEB_APPS));
}

#else   // !BUILDFLAG(IS_CHROMEOS_ASH)

// These tests test the new Web Apps system with next generation sync.
class SingleClientWebAppsSyncTest : public SyncTest {
 public:
  SingleClientWebAppsSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientWebAppsSyncTest() override = default;
};

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsSyncTest,
                       DisablingSelectedTypeDisablesModelType) {
  ASSERT_TRUE(SetupSync());
  syncer::ProfileSyncService* service = GetSyncService(0);
  syncer::SyncUserSettings* settings = service->GetUserSettings();
  ASSERT_TRUE(settings->GetSelectedTypes().Has(UserSelectableType::kApps));
  EXPECT_TRUE(service->GetActiveDataTypes().Has(syncer::WEB_APPS));

  settings->SetSelectedTypes(false, UserSelectableTypeSet());
  ASSERT_FALSE(settings->GetSelectedTypes().Has(UserSelectableType::kApps));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::WEB_APPS));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
