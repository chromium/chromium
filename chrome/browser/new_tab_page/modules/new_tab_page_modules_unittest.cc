// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"
#include "components/search/ntp_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp {

TEST(NewTabPageModulesTest, MakeModuleIdNames_NoDriveModule) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpRecipeTasksModule},
      /*disabled_features=*/{});

  const std::vector<std::pair<const std::string, int>> module_id_names =
      MakeModuleIdNames(false);
  ASSERT_EQ(1u, module_id_names.size());
}

TEST(NewTabPageModulesTest, MakeModuleIdNames_WithDriveModule) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpRecipeTasksModule,
                            ntp_features::kNtpDriveModule},
      /*disabled_features=*/{});

  const std::vector<std::pair<const std::string, int>> module_id_names =
      MakeModuleIdNames(true);
  ASSERT_EQ(2u, module_id_names.size());
}

#if !defined(OFFICIAL_BUILD)
TEST(NewTabPageModulesTest, MakeModuleIdNames_DummyModules) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{ntp_features::kNtpDummyModules},
      /*disabled_features=*/{});

  const std::vector<std::pair<const std::string, int>> module_id_names =
      MakeModuleIdNames(false);
  ASSERT_EQ(12u, module_id_names.size());
}
#endif

}  // namespace ntp
