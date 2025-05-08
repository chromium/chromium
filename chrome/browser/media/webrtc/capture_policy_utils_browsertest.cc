// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/capture_policy_utils.h"

#include <string>
#include <vector>

#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "url/origin.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {
constexpr char kValidIsolatedAppId1[] =
    "isolated-app://pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic";
}  // namespace

#if BUILDFLAG(IS_CHROMEOS)
namespace {

constexpr char kValidIsolatedAppId2[] =
    "isolated-app://aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";

struct TestParam {
  std::string allow_list_policy_name;
  std::vector<std::string> allow_listed_origins;
  std::string testing_url;
  bool expected_is_get_all_screens_media_allowed;
};

struct NoRefreshTestParam {
  std::string allow_list_policy_name;
  std::vector<std::string> original_allowlisted_origins;
  std::vector<std::string> updated_allowlisted_origins;
  std::vector<std::string> expected_allowed_origins;
  std::vector<std::string> expected_forbidden_origins;
};

}  // namespace

class SelectAllScreensTestBase : public policy::PolicyTest {
 public:
  explicit SelectAllScreensTestBase(const std::string& allow_list_policy_name)
      : allow_list_policy_name_(allow_list_policy_name) {}
  ~SelectAllScreensTestBase() override = default;

  SelectAllScreensTestBase(const SelectAllScreensTestBase&) = delete;
  SelectAllScreensTestBase& operator=(const SelectAllScreensTestBase&) = delete;

  void SetAllowedOriginsPolicy(
      const std::vector<std::string>& allow_listed_origins) {
    policy::PolicyMap policies;
    base::Value::List allowed_origins;
    for (const auto& allowed_origin : allow_listed_origins) {
      allowed_origins.Append(base::Value(allowed_origin));
    }
    PolicyTest::SetPolicy(&policies, allow_list_policy_name_.c_str(),
                          base::Value(std::move(allowed_origins)));
    provider_.UpdateChromePolicy(policies);
  }

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    SetAllowedOriginsPolicy(GetAllowedOrigins());
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  virtual std::vector<std::string> GetAllowedOrigins() const = 0;

 private:
  std::string allow_list_policy_name_;
};

class SelectAllScreensTest : public SelectAllScreensTestBase,
                             public testing::WithParamInterface<TestParam> {
 public:
  SelectAllScreensTest()
      : SelectAllScreensTestBase(GetParam().allow_list_policy_name) {}
  ~SelectAllScreensTest() override = default;

  SelectAllScreensTest(const SelectAllScreensTest&) = delete;
  SelectAllScreensTest& operator=(const SelectAllScreensTest&) = delete;

  std::vector<std::string> GetAllowedOrigins() const override {
    return GetParam().allow_listed_origins;
  }
};

IN_PROC_BROWSER_TEST_P(SelectAllScreensTest, SelectAllScreensTestOrigins) {
  base::test::TestFuture<bool> future;
  capture_policy::CheckGetAllScreensMediaAllowed(GURL(GetParam().testing_url),
                                                 future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(GetParam().expected_is_get_all_screens_media_allowed,
            future.Get<bool>());
}

INSTANTIATE_TEST_SUITE_P(
    SelectAllScreensTestWithParams,
    SelectAllScreensTest,
    testing::Values(TestParam({
                        .allow_list_policy_name =
                            policy::key::kMultiScreenCaptureAllowedForUrls,
                        .allow_listed_origins = {""},
                        .testing_url = "",
                        .expected_is_get_all_screens_media_allowed = false,
                    }),
                    TestParam({
                        .allow_list_policy_name =
                            policy::key::kMultiScreenCaptureAllowedForUrls,
                        .allow_listed_origins = {},
                        .testing_url = kValidIsolatedAppId1,
                        .expected_is_get_all_screens_media_allowed = false,
                    }),
                    TestParam({
                        .allow_list_policy_name =
                            policy::key::kMultiScreenCaptureAllowedForUrls,
                        .allow_listed_origins = {},
                        .testing_url = "",
                        .expected_is_get_all_screens_media_allowed = false,
                    }),
                    TestParam({
                        .allow_list_policy_name =
                            policy::key::kMultiScreenCaptureAllowedForUrls,
                        .allow_listed_origins = {"isolated-app://*"},
                        .testing_url = kValidIsolatedAppId1,
                        .expected_is_get_all_screens_media_allowed = false,
                    }),
                    TestParam({
                        .allow_list_policy_name =
                            policy::key::kMultiScreenCaptureAllowedForUrls,
                        .allow_listed_origins = {"*"},
                        .testing_url = kValidIsolatedAppId1,
                        .expected_is_get_all_screens_media_allowed = false,
                    }),
                    TestParam({
                        .allow_list_policy_name =
                            policy::key::kMultiScreenCaptureAllowedForUrls,
                        .allow_listed_origins = {kValidIsolatedAppId1},
                        .testing_url = kValidIsolatedAppId1,
                        .expected_is_get_all_screens_media_allowed = true,
                    })));

class SelectAllScreensDynamicRefreshTest
    : public SelectAllScreensTestBase,
      public testing::WithParamInterface<NoRefreshTestParam> {
 public:
  SelectAllScreensDynamicRefreshTest()
      : SelectAllScreensTestBase(GetParam().allow_list_policy_name) {}
  ~SelectAllScreensDynamicRefreshTest() override = default;

  explicit SelectAllScreensDynamicRefreshTest(const SelectAllScreensTest&) =
      delete;
  SelectAllScreensDynamicRefreshTest& operator=(const SelectAllScreensTest&) =
      delete;

  std::vector<std::string> GetAllowedOrigins() const override {
    return GetParam().original_allowlisted_origins;
  }

  void CheckExpectedAllowlistedAndForbiddenOrigins() {
    content::WebContents* current_web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    DCHECK(current_web_contents);
    for (const auto& expected_allowed_origin :
         GetParam().expected_allowed_origins) {
      base::test::TestFuture<bool> future;
      capture_policy::CheckGetAllScreensMediaAllowed(
          GURL(expected_allowed_origin), future.GetCallback());
      ASSERT_TRUE(future.Wait());
      EXPECT_TRUE(future.Get<bool>());
    }

    for (const auto& expected_forbidden_origin :
         GetParam().expected_forbidden_origins) {
      base::test::TestFuture<bool> future;
      capture_policy::CheckGetAllScreensMediaAllowed(
          GURL(expected_forbidden_origin), future.GetCallback());
      ASSERT_TRUE(future.Wait());
      EXPECT_FALSE(future.Get<bool>());
    }
  }
};

// This test checks that dynamic refresh (i.e. no origins can be added or
// removed after user login) is not happening.
IN_PROC_BROWSER_TEST_P(
    SelectAllScreensDynamicRefreshTest,
    SelectAllScreensEnabledWithMultipleAllowedOriginsNoDynamicRefresh) {
  CheckExpectedAllowlistedAndForbiddenOrigins();

  SetAllowedOriginsPolicy(GetParam().updated_allowlisted_origins);
  base::RunLoop().RunUntilIdle();
  CheckExpectedAllowlistedAndForbiddenOrigins();
}

INSTANTIATE_TEST_SUITE_P(
    SelectAllScreensDynamicRefreshTestWithParams,
    SelectAllScreensDynamicRefreshTest,
    testing::Values(NoRefreshTestParam({
                        .allow_list_policy_name =
                            policy::key::kMultiScreenCaptureAllowedForUrls,
                        .original_allowlisted_origins = {kValidIsolatedAppId1},
                        .updated_allowlisted_origins = {kValidIsolatedAppId1,
                                                        kValidIsolatedAppId2},
                        .expected_allowed_origins = {kValidIsolatedAppId1},
                        .expected_forbidden_origins = {kValidIsolatedAppId2},
                    }),
                    NoRefreshTestParam({
                        .allow_list_policy_name =
                            policy::key::kMultiScreenCaptureAllowedForUrls,
                        .original_allowlisted_origins = {},
                        .updated_allowlisted_origins = {kValidIsolatedAppId1,
                                                        kValidIsolatedAppId2},
                        .expected_allowed_origins = {},
                        .expected_forbidden_origins = {kValidIsolatedAppId1,
                                                       kValidIsolatedAppId2},
                    })));

class MultiCaptureTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<std::vector<std::string>, std::string>> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    provider_.SetDefaultReturns(/*is_initialization_complete_return=*/true,
                                /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    policy_map_.Set(policy::key::kMultiScreenCaptureAllowedForUrls,
                    policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                    policy::POLICY_SOURCE_CLOUD,
                    base::Value(AllowedOriginsList()),
                    /*external_data_fetcher=*/nullptr);
    provider_.UpdateChromePolicy(policy_map_);
  }

  const std::vector<std::string>& AllowedOrigins() const {
    return std::get<0>(GetParam());
  }

  base::Value::List AllowedOriginsList() const {
    base::Value::List allowed_origins;
    for (const auto& origin : AllowedOrigins()) {
      allowed_origins.Append(base::Value(origin));
    }
    return allowed_origins;
  }

  const std::string& CurrentOrigin() const { return std::get<1>(GetParam()); }

 protected:
  bool ExpectedIsMultiCaptureAllowed() {
    std::vector<std::string> allowed_urls = AllowedOrigins();
    return base::Contains(allowed_urls, CurrentOrigin());
  }

  bool ExpectedIsMultiCaptureAllowedForAnyUrl() {
    return !AllowedOrigins().empty();
  }

 private:
  policy::PolicyMap policy_map_;
  policy::MockConfigurationPolicyProvider provider_;
};

IN_PROC_BROWSER_TEST_P(MultiCaptureTest, IsMultiCaptureAllowedBasedOnPolicy) {
  base::test::TestFuture<bool> future;
  capture_policy::CheckGetAllScreensMediaAllowed(GURL(CurrentOrigin()),
                                                 future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(ExpectedIsMultiCaptureAllowed(), future.Get<bool>());
}

IN_PROC_BROWSER_TEST_P(MultiCaptureTest, IsMultiCaptureAllowedForAnyUrl) {
  base::test::TestFuture<bool> future;
  capture_policy::CheckGetAllScreensMediaAllowedForAnyOrigin(
      future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(ExpectedIsMultiCaptureAllowedForAnyUrl(), future.Get<bool>());
}

INSTANTIATE_TEST_SUITE_P(
    MultiCaptureTestCases,
    MultiCaptureTest,
    ::testing::Combine(
        // Allowed origins
        ::testing::ValuesIn({std::vector<std::string>{},
                             std::vector<std::string>{kValidIsolatedAppId1}}),
        // Origin to test
        ::testing::ValuesIn({std::string(kValidIsolatedAppId1),
                             std::string(kValidIsolatedAppId2)})));

#endif  // BUILDFLAG(IS_CHROMEOS)

using CaptureUtilsBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(CaptureUtilsBrowserTest, MultiCaptureServiceExists) {
  EXPECT_TRUE(capture_policy::GetMultiCaptureService());
}

IN_PROC_BROWSER_TEST_F(CaptureUtilsBrowserTest,
                       NoPolicySetMultiCaptureServiceMaybeExists) {
  base::test::TestFuture<bool> future;
  capture_policy::CheckGetAllScreensMediaAllowed(GURL(kValidIsolatedAppId1),
                                                 future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get<bool>());
}
