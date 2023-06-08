// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_util.h"

#include "base/test/values_test_util.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bruschetta {

TEST(BruschettaUtilTest, SortInstallableConfigs) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  profile.GetPrefs()->SetDict(prefs::kBruschettaVMConfiguration,
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
  std::vector<InstallableConfig> configs =
      GetInstallableConfigs(&profile).extract();
  ASSERT_EQ(2U, configs.size());
  EXPECT_EQ("vm_config_abc", configs[0].first);
  SortInstallableConfigs(&configs);
  EXPECT_EQ("vm_config_def", configs[0].first);
}

}  // namespace bruschetta
