// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/chromeos_buildflags.h"
#include "chrome/browser/sync/test/integration/apps_sync_test_base.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service_impl.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::UserSelectableType;
using syncer::UserSelectableTypeSet;

namespace {

// TODO(https://crbug.com/1280212): See if this test can be enabled on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

// See also TwoClientExtensionSettingsAndAppSettingsSyncTest.
class SingleClientAppSettingsSyncTest : public AppsSyncTestBase {
 public:
  SingleClientAppSettingsSyncTest() : AppsSyncTestBase(SINGLE_CLIENT) {}
  ~SingleClientAppSettingsSyncTest() override = default;
};

IN_PROC_BROWSER_TEST_F(SingleClientAppSettingsSyncTest, Basics) {
  ASSERT_TRUE(SetupSync());
  syncer::SyncServiceImpl* service = GetSyncService(0);
  syncer::SyncUserSettings* settings = service->GetUserSettings();
  EXPECT_TRUE(settings->GetSelectedTypes().Has(UserSelectableType::kApps));
  EXPECT_TRUE(service->GetActiveDataTypes().Has(syncer::APP_SETTINGS));

  settings->SetSelectedTypes(false, UserSelectableTypeSet());
  EXPECT_FALSE(settings->GetSelectedTypes().Has(UserSelectableType::kApps));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::APP_SETTINGS));
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
