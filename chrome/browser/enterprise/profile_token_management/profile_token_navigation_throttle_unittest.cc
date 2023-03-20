// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/profile_token_management/profile_token_navigation_throttle.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/profile_token_management/token_management_features.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace profile_token_management {

using testing::_;

namespace {

class MockTokenInfoGetter
    : public ProfileTokenNavigationThrottle::TokenInfoGetter {
 public:
  MockTokenInfoGetter() = default;
  ~MockTokenInfoGetter() override = default;

  MOCK_METHOD(
      void,
      GetTokenInfo,
      (content::NavigationHandle*,
       base::OnceCallback<void(const std::string&, const std::string&)>),
      (override));
};

}  // namespace

class ProfileTokenNavigationThrottleTest : public BrowserWithTestWindowTest {
 public:
  ProfileTokenNavigationThrottleTest() = default;

  ~ProfileTokenNavigationThrottleTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    // Create the first tab so that web_contents() exists.
    AddTab(browser(), GURL("http://foo/1"));
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* main_frame() const {
    return web_contents()->GetPrimaryMainFrame();
  }
};

TEST_F(ProfileTokenNavigationThrottleTest,
       ProfileTokenManagementFeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kEnableProfileTokenManagement);

  content::MockNavigationHandle test_handle(GURL("https://www.example.test/"),
                                            main_frame());

  auto throttle =
      ProfileTokenNavigationThrottle::MaybeCreateThrottleFor(&test_handle);
  ASSERT_EQ(nullptr, throttle.get());
}

TEST_F(ProfileTokenNavigationThrottleTest, ProfileCreationDisallowed) {
  base::test::ScopedFeatureList features(
      features::kEnableProfileTokenManagement);
  g_browser_process->local_state()->SetBoolean(prefs::kBrowserAddPersonEnabled,
                                               false);
  ASSERT_FALSE(profiles::IsProfileCreationAllowed());

  content::MockNavigationHandle test_handle(GURL("https://www.example.test/"),
                                            main_frame());

  auto throttle =
      ProfileTokenNavigationThrottle::MaybeCreateThrottleFor(&test_handle);
  ASSERT_EQ(nullptr, throttle.get());
}

TEST_F(ProfileTokenNavigationThrottleTest, NoThrottlingWithUnsupportedHost) {
  base::test::ScopedFeatureList features(
      features::kEnableProfileTokenManagement);
  content::MockNavigationHandle test_handle(GURL("https://notasupported.host/"),
                                            main_frame());
  std::unique_ptr<MockTokenInfoGetter> mock_info_getter =
      std::make_unique<MockTokenInfoGetter>();
  EXPECT_CALL(*mock_info_getter, GetTokenInfo(_, _)).Times(0);

  auto throttle = std::make_unique<ProfileTokenNavigationThrottle>(
      &test_handle, std::move(mock_info_getter));

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse().action());
}

TEST_F(ProfileTokenNavigationThrottleTest, ThrottlingWithSupportedHost) {
  base::test::ScopedFeatureList features(
      features::kEnableProfileTokenManagement);
  content::MockNavigationHandle test_handle(
      GURL(std::string("https://") + kTestHost), main_frame());
  std::unique_ptr<MockTokenInfoGetter> mock_info_getter =
      std::make_unique<MockTokenInfoGetter>();
  EXPECT_CALL(*mock_info_getter, GetTokenInfo(&test_handle, _)).Times(1);

  auto throttle = std::make_unique<ProfileTokenNavigationThrottle>(
      &test_handle, std::move(mock_info_getter));

  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillProcessResponse().action());
}

}  // namespace profile_token_management
