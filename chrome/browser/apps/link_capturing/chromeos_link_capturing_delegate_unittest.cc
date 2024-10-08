// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/chromeos_link_capturing_delegate.h"

#include "ash/constants/web_app_id_constants.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {
namespace {

using ChromeOsLinkCapturingDelegateTest = testing::Test;

TEST_F(ChromeOsLinkCapturingDelegateTest, GetLaunchAppId_Preferred) {
  AppIdsToLaunchForUrl app_ids_to_launch;
  app_ids_to_launch.candidates = {"foo", "bar"};
  app_ids_to_launch.preferred = "foo";

  std::optional<std::string> launch_id =
      ChromeOsLinkCapturingDelegate::GetLaunchAppId(
          app_ids_to_launch, /*is_navigation_from_link=*/true);

  ASSERT_EQ(launch_id, "foo");
}

TEST_F(ChromeOsLinkCapturingDelegateTest, GetLaunchAppId_NoPreferred) {
  AppIdsToLaunchForUrl app_ids_to_launch;
  app_ids_to_launch.candidates = {"foo", "bar"};

  std::optional<std::string> launch_id =
      ChromeOsLinkCapturingDelegate::GetLaunchAppId(
          app_ids_to_launch, /*is_navigation_from_link=*/true);

  ASSERT_EQ(launch_id, std::nullopt);
}

}  // namespace
}  // namespace apps
