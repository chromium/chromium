// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/supervised_user/supervised_user_browsertest_base.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// This unit consists of test cases that are applicable to both Android and
// Desktop: interactions between regular users and users supervised by Family
// Link.
namespace supervised_user {
namespace {
// A suite for regular users (most of the time should assert that features are
// initially disabled, unless users transition to supervised state).
class RegularUserUrlFilteringServiceCommonBrowserTest
    : public SupervisedUserBrowserTestBase {};
IN_PROC_BROWSER_TEST_F(RegularUserUrlFilteringServiceCommonBrowserTest,
                       UrlFilterIsOffByDefault) {
  EXPECT_EQ(WebFilterType::kDisabled,
            SupervisedUserServiceFactory::GetForProfile(GetProfile())
                ->GetURLFilter()
                ->GetWebFilterType());
}
IN_PROC_BROWSER_TEST_F(RegularUserUrlFilteringServiceCommonBrowserTest,
                       EnablingFamilyLinkSupervisionEnablesUrlFiltering) {
  EnableParentalControls(*GetProfile()->GetPrefs());
  EXPECT_EQ(WebFilterType::kTryToBlockMatureSites,
            SupervisedUserServiceFactory::GetForProfile(GetProfile())
                ->GetURLFilter()
                ->GetWebFilterType());
}

// TODO(crbug.com/468935875) - Re-enable on ChromeOS with user type support.
// Tests in ChromeOS require proper user account, which is not available at
// the time for these tests.
#if !BUILDFLAG(IS_CHROMEOS)

// A suite for supervised users configured by Family Link (most of the time
// should assert that features are initially enabled, unless users transition to
// regular state).
class FamilyLinkUrlFilteringServiceCommonBrowserTest
    : public SupervisedUserBrowserTestBase {
 public:
  FamilyLinkUrlFilteringServiceCommonBrowserTest() {
    SetInitialSupervisedUserState(
        InitialSupervisedUserState{.family_link_parental_controls = true});
  }
};
IN_PROC_BROWSER_TEST_F(FamilyLinkUrlFilteringServiceCommonBrowserTest,
                       UrlFilterIsOnByDefault) {
  EXPECT_EQ(WebFilterType::kTryToBlockMatureSites,
            SupervisedUserServiceFactory::GetForProfile(GetProfile())
                ->GetURLFilter()
                ->GetWebFilterType());
}
IN_PROC_BROWSER_TEST_F(FamilyLinkUrlFilteringServiceCommonBrowserTest,
                       UrlFilterCanBeConfiguredByParent) {
  ASSERT_EQ(WebFilterType::kTryToBlockMatureSites,
            SupervisedUserServiceFactory::GetForProfile(GetProfile())
                ->GetURLFilter()
                ->GetWebFilterType());
  // Starting from kTryToBlockMatureSites, this sequence of changes covers all
  // possible back-and-forth transitions between kTryToBlockMatureSites,
  // kBlockAllSites, and kAllowAllSites (starting with kTryToBlockMatureSites,
  // asserted above).
  std::vector<WebFilterType> web_filter_types = {
      WebFilterType::kAllowAllSites,
      WebFilterType::kCertainSites,
      WebFilterType::kTryToBlockMatureSites,
      WebFilterType::kCertainSites,
      WebFilterType::kAllowAllSites,
      WebFilterType::kTryToBlockMatureSites};
  for (const auto& web_filter_type : web_filter_types) {
    supervised_user_test_util::SetWebFilterType(GetProfile(), web_filter_type);
    EXPECT_EQ(web_filter_type,
              SupervisedUserServiceFactory::GetForProfile(GetProfile())
                  ->GetURLFilter()
                  ->GetWebFilterType());
  }
}
IN_PROC_BROWSER_TEST_F(FamilyLinkUrlFilteringServiceCommonBrowserTest,
                       DisablingFamilyLinkSupervisionDisablesUrlFiltering) {
  DisableParentalControls(*GetProfile()->GetPrefs());
  EXPECT_EQ(WebFilterType::kDisabled,
            SupervisedUserServiceFactory::GetForProfile(GetProfile())
                ->GetURLFilter()
                ->GetWebFilterType());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
}  // namespace
}  // namespace supervised_user
