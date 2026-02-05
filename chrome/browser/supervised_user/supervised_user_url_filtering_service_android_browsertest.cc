// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"

#include <memory>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/android/supervised_user_service_platform_delegate.h"
#include "chrome/browser/supervised_user/family_link_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_browsertest_base.h"
#include "chrome/browser/supervised_user/supervised_user_metrics_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/browser/supervised_user/supervised_user_url_filtering_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/google/core/common/google_switches.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/safe_search_api/url_checker_client.h"
#include "components/supervised_user/core/browser/android/content_filters_observer_bridge.h"
#include "components/supervised_user/core/browser/kids_chrome_management_url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// This unit consists of test cases that are applicable to both Android only,
// and covers interactions between regular users, Family-Link supervised users
// and users locally supervised by Android parental controls. See
// supervised_user_url_filtering_service_common_browsertest.cc
// for the corresponding test suite applicable to both Android and Desktop, but
// without Android parental controls.
namespace supervised_user {
namespace {

class SupervisedUserBrowserCasedTestBase
    : public SupervisedUserBrowserTestBase,
      public base::test::WithFeatureOverride {
 protected:
  SupervisedUserBrowserCasedTestBase()
      : base::test::WithFeatureOverride(kSupervisedUserUseUrlFilteringService) {
  }
};

// A suite for regular users (most of the time should assert that features are
// initially disabled and provide neutral, default browsing experience, unless
// users transition to supervised state).
class RegularUserUrlFilteringServiceAndroidBrowserTest
    : public SupervisedUserBrowserCasedTestBase {};

IN_PROC_BROWSER_TEST_P(RegularUserUrlFilteringServiceAndroidBrowserTest,
                       EnablingAndroidParentalControlsEnablesUrlFiltering) {
  GetDeviceParentalControls().SetBrowserContentFiltersEnabledForTesting(true);
  EXPECT_EQ(
      WebFilterType::kTryToBlockMatureSites,
      SupervisedUserUrlFilteringServiceFactory::GetForProfile(GetProfile())
          ->GetWebFilterType());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    RegularUserUrlFilteringServiceAndroidBrowserTest);

// A suite for supervised users with configured by Family Link.
class FamilyLinkUrlFilteringServiceAndroidBrowserTest
    : public SupervisedUserBrowserCasedTestBase {
 public:
  FamilyLinkUrlFilteringServiceAndroidBrowserTest() {
    SetInitialSupervisedUserState(
        InitialSupervisedUserState{.family_link_parental_controls = true});
  }
};

IN_PROC_BROWSER_TEST_P(FamilyLinkUrlFilteringServiceAndroidBrowserTest,
                       AndroidParentalControlsAreIgnored) {
  if (base::FeatureList::IsEnabled(kSupervisedUserUseUrlFilteringService)) {
    GTEST_SKIP() << "Test for legacy implementation only.";
  }

  // Android parental controls only can set web filter to
  // kTryToBlockMatureSites; so let's reconfigure to something else to make sure
  // that the change is ignored.
  supervised_user_test_util::SetWebFilterType(GetProfile(),
                                              WebFilterType::kAllowAllSites);
  ASSERT_EQ(
      WebFilterType::kAllowAllSites,
      SupervisedUserUrlFilteringServiceFactory::GetForProfile(GetProfile())
          ->GetWebFilterType());

  // Setting is ignored.
  GetDeviceParentalControls().SetBrowserContentFiltersEnabledForTesting(true);
  EXPECT_EQ(
      WebFilterType::kAllowAllSites,
      SupervisedUserUrlFilteringServiceFactory::GetForProfile(GetProfile())
          ->GetWebFilterType());
}

IN_PROC_BROWSER_TEST_P(FamilyLinkUrlFilteringServiceAndroidBrowserTest,
                       AndroidParentalControlsArePreferred) {
  if (!base::FeatureList::IsEnabled(kSupervisedUserUseUrlFilteringService)) {
    GTEST_SKIP() << "Test for experimental implementation only.";
  }

  // Android parental controls only can set web filter to
  // kTryToBlockMatureSites; so let's reconfigure to something else to make sure
  // that the change is ignored.
  supervised_user_test_util::SetWebFilterType(GetProfile(),
                                              WebFilterType::kAllowAllSites);
  ASSERT_EQ(
      WebFilterType::kAllowAllSites,
      SupervisedUserUrlFilteringServiceFactory::GetForProfile(GetProfile())
          ->GetWebFilterType());

  GetDeviceParentalControls().SetBrowserContentFiltersEnabledForTesting(true);
  EXPECT_EQ(
      WebFilterType::kTryToBlockMatureSites,
      SupervisedUserUrlFilteringServiceFactory::GetForProfile(GetProfile())
          ->GetWebFilterType());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    FamilyLinkUrlFilteringServiceAndroidBrowserTest);

// A suite for supervised users configured by Android parental controls.
class AndroidParentalControlsUrlFilteringServiceAndroidBrowserTest
    : public SupervisedUserBrowserCasedTestBase {
 public:
  AndroidParentalControlsUrlFilteringServiceAndroidBrowserTest() {
    SetInitialSupervisedUserState(
        InitialSupervisedUserState{.android_parental_controls = {
                                       .browser_filter = true,
                                       .search_filter = true,
                                   }});
  }
};

IN_PROC_BROWSER_TEST_P(
    AndroidParentalControlsUrlFilteringServiceAndroidBrowserTest,
    FamilyLinkDisablesAndroidParentalControls) {
  ASSERT_TRUE(
      AreAndroidParentalControlsEffectiveForTesting(*GetProfile()->GetPrefs()));
  ASSERT_EQ(
      WebFilterType::kTryToBlockMatureSites,
      SupervisedUserUrlFilteringServiceFactory::GetForProfile(GetProfile())
          ->GetWebFilterType());

  EnableParentalControls(*GetProfile()->GetPrefs());

  EXPECT_FALSE(
      AreAndroidParentalControlsEffectiveForTesting(*GetProfile()->GetPrefs()));
  EXPECT_EQ(
      WebFilterType::kTryToBlockMatureSites,
      SupervisedUserUrlFilteringServiceFactory::GetForProfile(GetProfile())
          ->GetWebFilterType());
}

IN_PROC_BROWSER_TEST_P(
    AndroidParentalControlsUrlFilteringServiceAndroidBrowserTest,
    DisablingAndroidParentalControlsSupervisionDisablesUrlFiltering) {
  GetDeviceParentalControls().SetBrowserContentFiltersEnabledForTesting(false);
  GetDeviceParentalControls().SetSearchContentFiltersEnabledForTesting(false);
  EXPECT_EQ(
      WebFilterType::kDisabled,
      SupervisedUserUrlFilteringServiceFactory::GetForProfile(GetProfile())
          ->GetWebFilterType());
}

IN_PROC_BROWSER_TEST_P(
    AndroidParentalControlsUrlFilteringServiceAndroidBrowserTest,
    DisablingWebFilterOnlyPutsUrlFilterininInAllowAllMode) {
  if (!base::FeatureList::IsEnabled(kSupervisedUserUseUrlFilteringService)) {
    GTEST_SKIP() << "Test for experimental implementation only.";
  }

  GetDeviceParentalControls().SetBrowserContentFiltersEnabledForTesting(false);
  GetDeviceParentalControls().SetSearchContentFiltersEnabledForTesting(true);
  EXPECT_EQ(
      WebFilterType::kAllowAllSites,
      SupervisedUserUrlFilteringServiceFactory::GetForProfile(GetProfile())
          ->GetWebFilterType());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    AndroidParentalControlsUrlFilteringServiceAndroidBrowserTest);

}  // namespace
}  // namespace supervised_user
