// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/chromeos_buildflags.h"
#include "chrome/browser/sync/test/integration/apps_sync_test_base.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
using syncer::UserSelectableOsType;
using syncer::UserSelectableOsTypeSet;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
using syncer::UserSelectableType;
using syncer::UserSelectableTypeSet;

namespace {

// See also TwoClientExtensionSettingsAndAppSettingsSyncTest.
class SingleClientAppSettingsSyncTest : public AppsSyncTestBase {
 public:
  SingleClientAppSettingsSyncTest() : AppsSyncTestBase(SINGLE_CLIENT) {}
  ~SingleClientAppSettingsSyncTest() override = default;
};

IN_PROC_BROWSER_TEST_F(SingleClientAppSettingsSyncTest, Basics) {
  ASSERT_TRUE(SetupClients());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Apps sync is controlled by a dedicated preference on Lacros,
  // corresponding to the Apps toggle in OS Sync settings.
  // We need to enable the Apps toggle for both Client
  if (base::FeatureList::IsEnabled(syncer::kSyncChromeOSAppsToggleSharing)) {
    GetSyncService(0)->GetUserSettings()->SetAppsSyncEnabledByOs(true);
  }
#endif
  ASSERT_TRUE(SetupSync());
  syncer::SyncServiceImpl* service = GetSyncService(0);
  syncer::SyncUserSettings* settings = service->GetUserSettings();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(settings->GetSelectedTypes().Has(UserSelectableType::kApps));
  EXPECT_TRUE(service->GetActiveDataTypes().Has(syncer::APP_SETTINGS));

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Apps sync is controlled by a dedicated preference on Lacros,
  // corresponding to the Apps toggle in OS Sync settings if
  // kSyncChromeOSAppsToggleSharing is enabled. Disabling Apps sync requires
  // disabling Apps toggle in OS.
  if (base::FeatureList::IsEnabled(syncer::kSyncChromeOSAppsToggleSharing)) {
    settings->SetAppsSyncEnabledByOs(false);
  } else {
    settings->SetSelectedTypes(false, UserSelectableTypeSet());
  }
#else
  settings->SetSelectedTypes(false, UserSelectableTypeSet());
#endif

  EXPECT_FALSE(settings->GetSelectedTypes().Has(UserSelectableType::kApps));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::APP_SETTINGS));
#else
  EXPECT_TRUE(
      settings->GetSelectedOsTypes().Has(UserSelectableOsType::kOsApps));
  EXPECT_TRUE(service->GetActiveDataTypes().Has(syncer::APP_SETTINGS));

  settings->SetSelectedOsTypes(false, UserSelectableOsTypeSet());
  EXPECT_FALSE(
      settings->GetSelectedOsTypes().Has(UserSelectableOsType::kOsApps));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::APP_SETTINGS));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace
