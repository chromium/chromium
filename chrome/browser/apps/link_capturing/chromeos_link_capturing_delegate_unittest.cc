// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/chromeos_link_capturing_delegate.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {
namespace {

using ChromeOsLinkCapturingDelegateTest = testing::Test;

TEST_F(ChromeOsLinkCapturingDelegateTest, GetLaunchAppId_Preferred) {
  AppIdsToLaunchForUrl app_ids_to_launch;
  app_ids_to_launch.candidates = {"foo", "bar"};
  app_ids_to_launch.preferred = "foo";

  absl::optional<std::string> launch_id =
      ChromeOsLinkCapturingDelegate::GetLaunchAppId(
          app_ids_to_launch, /*is_navigation_from_link=*/true,
          /*source_app_id=*/absl::nullopt);

  ASSERT_EQ(launch_id, "foo");
}

TEST_F(ChromeOsLinkCapturingDelegateTest, GetLaunchAppId_NoPreferred) {
  AppIdsToLaunchForUrl app_ids_to_launch;
  app_ids_to_launch.candidates = {"foo", "bar"};

  absl::optional<std::string> launch_id =
      ChromeOsLinkCapturingDelegate::GetLaunchAppId(
          app_ids_to_launch, /*is_navigation_from_link=*/true,
          /*source_app_id=*/absl::nullopt);

  ASSERT_EQ(launch_id, absl::nullopt);
}

TEST_F(ChromeOsLinkCapturingDelegateTest,
       GetLaunchAppId_AppToApp_OneCandidate) {
  base::test::ScopedFeatureList feature_list{
      apps::features::kAppToAppLinkCapturing};
  AppIdsToLaunchForUrl app_ids_to_launch;
  app_ids_to_launch.candidates = {"foo"};

  absl::optional<std::string> launch_id =
      ChromeOsLinkCapturingDelegate::GetLaunchAppId(
          app_ids_to_launch, /*is_navigation_from_link=*/true,
          /*source_app_id=*/"bar");

  ASSERT_EQ(launch_id, "foo");
}

TEST_F(ChromeOsLinkCapturingDelegateTest,
       GetLaunchAppId_AppToApp_OneCandidate_NoSourceApp) {
  base::test::ScopedFeatureList feature_list{
      apps::features::kAppToAppLinkCapturing};
  AppIdsToLaunchForUrl app_ids_to_launch;
  app_ids_to_launch.candidates = {"foo"};

  absl::optional<std::string> launch_id =
      ChromeOsLinkCapturingDelegate::GetLaunchAppId(
          app_ids_to_launch, /*is_navigation_from_link=*/true,
          /*source_app_id=*/absl::nullopt);

  ASSERT_EQ(launch_id, absl::nullopt);
}

TEST_F(ChromeOsLinkCapturingDelegateTest,
       GetLaunchAppId_AppToApp_TwoCandidates) {
  base::test::ScopedFeatureList feature_list{
      apps::features::kAppToAppLinkCapturing};
  AppIdsToLaunchForUrl app_ids_to_launch;
  app_ids_to_launch.candidates = {"foo", "bar"};

  absl::optional<std::string> launch_id =
      ChromeOsLinkCapturingDelegate::GetLaunchAppId(
          app_ids_to_launch, /*is_navigation_from_link=*/true,
          /*source_app_id=*/"baz");

  ASSERT_EQ(launch_id, absl::nullopt);
}

TEST_F(ChromeOsLinkCapturingDelegateTest,
       GetLaunchAppId_AppToAppWorkspace_OneWorkspaceCandidate) {
  base::test::ScopedFeatureList feature_list{
      apps::features::kAppToAppLinkCapturingWorkspaceApps};
  AppIdsToLaunchForUrl app_ids_to_launch;
  app_ids_to_launch.candidates = {web_app::kGoogleDocsAppId};

  absl::optional<std::string> launch_id =
      ChromeOsLinkCapturingDelegate::GetLaunchAppId(
          app_ids_to_launch, /*is_navigation_from_link=*/true,
          /*source_app_id=*/web_app::kGoogleDriveAppId);

  ASSERT_EQ(launch_id, web_app::kGoogleDocsAppId);
}

TEST_F(ChromeOsLinkCapturingDelegateTest,
       GetLaunchAppId_AppToAppWorkspace_OnlyCaptureFromWorkspaceSource) {
  base::test::ScopedFeatureList feature_list{
      apps::features::kAppToAppLinkCapturingWorkspaceApps};
  AppIdsToLaunchForUrl app_ids_to_launch;
  app_ids_to_launch.candidates = {web_app::kGoogleDocsAppId};

  absl::optional<std::string> launch_id =
      ChromeOsLinkCapturingDelegate::GetLaunchAppId(
          app_ids_to_launch, /*is_navigation_from_link=*/true,
          /*source_app_id=*/"nonworkspaceapp");

  ASSERT_EQ(launch_id, absl::nullopt);
}

TEST_F(
    ChromeOsLinkCapturingDelegateTest,
    GetLaunchAppId_AppToAppWorkspace_MultipleCandidates_UsesWorkspaceByDefault) {
  base::test::ScopedFeatureList feature_list{
      apps::features::kAppToAppLinkCapturingWorkspaceApps};

  // When there are multiple candidates, and the link is clicked from a
  // workspace app, default to using any workspace app in the list.
  AppIdsToLaunchForUrl app_ids_to_launch;
  app_ids_to_launch.candidates = {"app", web_app::kGoogleDocsAppId,
                                  "anotherapp"};

  absl::optional<std::string> launch_id =
      ChromeOsLinkCapturingDelegate::GetLaunchAppId(
          app_ids_to_launch, /*is_navigation_from_link=*/true,
          /*source_app_id=*/web_app::kGoogleDriveAppId);

  ASSERT_EQ(launch_id, web_app::kGoogleDocsAppId);
}

TEST_F(ChromeOsLinkCapturingDelegateTest,
       GetLaunchAppId_AppToAppWorkspace_TwoCandidates_RespectsPreference) {
  base::test::ScopedFeatureList feature_list{
      apps::features::kAppToAppLinkCapturingWorkspaceApps};

  // Respect any user preference that is set, even if there is a workspace app
  // as a candidate.
  AppIdsToLaunchForUrl app_ids_to_launch;
  app_ids_to_launch.candidates = {web_app::kGoogleDocsAppId, "anotherapp"};
  app_ids_to_launch.preferred = "anotherapp";

  absl::optional<std::string> launch_id =
      ChromeOsLinkCapturingDelegate::GetLaunchAppId(
          app_ids_to_launch, /*is_navigation_from_link=*/true,
          /*source_app_id=*/web_app::kGoogleDriveAppId);

  ASSERT_EQ(launch_id, "anotherapp");
}

}  // namespace
}  // namespace apps
