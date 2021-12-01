// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/values.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/api_guard_delegate.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension_urls.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr char kExtensionId[] = "gogonhoemckpdpadfnjnpgbjpbjnodgc";

}  // namespace

class ApiGuardDelegateTest : public testing::Test {
 public:
  ApiGuardDelegateTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}
  ~ApiGuardDelegateTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(ApiGuardDelegateTest, IsExtensionForceInstalledFalse) {
  EXPECT_FALSE(ApiGuardDelegate::Factory::Create()->IsExtensionForceInstalled(
      &profile_, kExtensionId));
}

TEST_F(ApiGuardDelegateTest, IsExtensionForceInstalledTrue) {
  {
    extensions::ExtensionManagementPrefUpdater<
        sync_preferences::TestingPrefServiceSyncable>
        updater(profile_.GetTestingPrefService());
    updater.SetIndividualExtensionAutoInstalled(
        kExtensionId, extension_urls::kChromeWebstoreUpdateURL,
        /*forced=*/true);
  }

  EXPECT_TRUE(ApiGuardDelegate::Factory::Create()->IsExtensionForceInstalled(
      &profile_, kExtensionId));

  // Make sure IsExtensionForceInstalled() doesn't return true blindly.
  EXPECT_FALSE(ApiGuardDelegate::Factory::Create()->IsExtensionForceInstalled(
      &profile_, "alnedpmllcfpgldkagbfbjkloonjlfjb"));
}

}  // namespace chromeos
