// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_util.h"

#include "base/test/values_test_util.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace bruschetta {

namespace {

void SetVMConfigPref(Profile* profile) {
  profile->GetPrefs()->SetDict(prefs::kBruschettaVMConfiguration,
                               base::test::ParseJsonDict(R"(
    {
      "vm_config_abc": {
          "name": "abc",
          "enabled_state": 2,
          "display_order": 1
      },
      "vm_config_def": {
          "name": "def",
          "enabled_state": 2
      }
    }
  )"));
}

void SetInstallerConfigPref(Profile* profile) {
  profile->GetPrefs()->SetDict(prefs::kBruschettaInstallerConfiguration,
                               base::test::ParseJsonDict(R"(
    {
      "display_name": "Display name",
      "learn_more_url": "https://example.com/learn_more"
    }
  )"));
}

}  // namespace

TEST(BruschettaUtilTest, SortInstallableConfigs) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  SetVMConfigPref(&profile);
  std::vector<InstallableConfig> configs =
      GetInstallableConfigs(&profile).extract();
  ASSERT_EQ(2U, configs.size());
  EXPECT_EQ("vm_config_abc", configs[0].first);
  SortInstallableConfigs(&configs);
  EXPECT_EQ("vm_config_def", configs[0].first);
}

TEST(BruschettaUtilTest, GetOverallVmNameFromPolicy) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  EXPECT_EQ(GetOverallVmName(&profile),
            l10n_util::GetStringUTF16(IDS_BRUSCHETTA_NAME));
  SetVMConfigPref(&profile);
  EXPECT_EQ(GetOverallVmName(&profile), u"def");
  SetInstallerConfigPref(&profile);
  EXPECT_EQ(GetOverallVmName(&profile), u"Display name");
}

TEST(BruschettaUtilTest, GetLearnMoreUrl) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  EXPECT_FALSE(GetLearnMoreUrl(&profile).is_valid());
  SetInstallerConfigPref(&profile);
  EXPECT_EQ(GetLearnMoreUrl(&profile), GURL("https://example.com/learn_more"));
}

TEST(BruschettaUtilTest, GetDisplayNameNotInPrefs) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ASSERT_EQ(GetDisplayName(&profile, GetBruschettaAlphaId()), "bru");
}

TEST(BruschettaUtilTest, GetDisplayNameInPrefs) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  profile.GetPrefs()->SetDict(prefs::kBruschettaVMConfiguration,
                              base::test::ParseJsonDict(R"(
    {
      "config": {
          "name": "Display Name",
          "enabled_state": 2,
          "display_order": 1
      },
    }
  )"));
  profile.GetPrefs()->SetList(guest_os::prefs::kGuestOsContainers,
                              base::test::ParseJsonList(R"(
    [
      {
        "vm_name": "bru",
        "container_name": "penguin",
        "bruschetta_config_id": "config"
      }
    ]
  )"));

  ASSERT_EQ(GetDisplayName(&profile, GetBruschettaAlphaId()), "Display Name");
}

}  // namespace bruschetta
