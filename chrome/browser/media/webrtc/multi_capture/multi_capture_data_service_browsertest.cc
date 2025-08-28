// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/multi_capture/multi_capture_data_service.h"

#include <string>
#include <vector>

#include "chrome/browser/media/webrtc/multi_capture/multi_capture_data_service.h"
#include "chrome/browser/media/webrtc/multi_capture/multi_capture_data_service_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kValidIsolatedAppId1[] =
    "isolated-app://pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic";
}  // namespace

namespace {

constexpr char kValidIsolatedAppId2[] =
    "isolated-app://aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";

struct TestParam {
  std::string allowlist_policy_name;
  std::vector<std::string> allowlisted_origins;
  std::string testing_url;
  bool expected_is_get_all_screens_media_allowed;
};

struct NoRefreshTestParam {
  std::string allowlist_policy_name;
  std::vector<std::string> original_allowlisted_origins;
  std::vector<std::string> updated_allowlisted_origins;
  std::vector<std::string> expected_allowed_origins;
  std::vector<std::string> expected_forbidden_origins;
};

}  // namespace

class MultiCaptureDataTestBase : public policy::PolicyTest {
 public:
  explicit MultiCaptureDataTestBase(const std::string& allowlist_policy_name)
      : allowlist_policy_name_(allowlist_policy_name) {}
  ~MultiCaptureDataTestBase() override = default;

  MultiCaptureDataTestBase(const MultiCaptureDataTestBase&) = delete;
  MultiCaptureDataTestBase& operator=(const MultiCaptureDataTestBase&) = delete;

  void SetAllowedOriginsPolicy(
      const std::vector<std::string>& allowlisted_origins) {
    policy::PolicyMap policies;
    base::Value::List allowed_origins;
    for (const auto& allowed_origin : allowlisted_origins) {
      allowed_origins.Append(base::Value(allowed_origin));
    }
    PolicyTest::SetPolicy(&policies, allowlist_policy_name_.c_str(),
                          base::Value(std::move(allowed_origins)));
    provider_.UpdateChromePolicy(policies);
  }

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    SetAllowedOriginsPolicy(GetAllowedOrigins());
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  virtual std::vector<std::string> GetAllowedOrigins() const = 0;

 private:
  std::string allowlist_policy_name_;
};

class MultiCaptureDataTest : public MultiCaptureDataTestBase,
                             public testing::WithParamInterface<TestParam> {
 public:
  MultiCaptureDataTest()
      : MultiCaptureDataTestBase(GetParam().allowlist_policy_name) {}
  ~MultiCaptureDataTest() override = default;

  MultiCaptureDataTest(const MultiCaptureDataTest&) = delete;
  MultiCaptureDataTest& operator=(const MultiCaptureDataTest&) = delete;

  std::vector<std::string> GetAllowedOrigins() const override {
    return GetParam().allowlisted_origins;
  }
};

IN_PROC_BROWSER_TEST_P(MultiCaptureDataTest, TestExactOrigins) {
  EXPECT_EQ(GetParam().expected_is_get_all_screens_media_allowed,
            multi_capture::MultiCaptureDataServiceFactory::GetForBrowserContext(
                browser()->profile())
                ->IsMultiCaptureAllowed(GURL(GetParam().testing_url)));
}

INSTANTIATE_TEST_SUITE_P(
    MultiCaptureDataTestWithParams,
    MultiCaptureDataTest,
    testing::Values(TestParam({
                        .allowlist_policy_name =
                            policy::key::kMultiScreenCaptureAllowedForUrls,
                        .allowlisted_origins = {""},
                        .testing_url = "",
                        .expected_is_get_all_screens_media_allowed = false,
                    }),
                    TestParam({
                        .allowlist_policy_name =
                            policy::key::kMultiScreenCaptureAllowedForUrls,
                        .allowlisted_origins = {},
                        .testing_url = kValidIsolatedAppId1,
                        .expected_is_get_all_screens_media_allowed = false,
                    }),
                    TestParam({
                        .allowlist_policy_name =
                            policy::key::kMultiScreenCaptureAllowedForUrls,
                        .allowlisted_origins = {},
                        .testing_url = "",
                        .expected_is_get_all_screens_media_allowed = false,
                    }),
                    TestParam({
                        .allowlist_policy_name =
                            policy::key::kMultiScreenCaptureAllowedForUrls,
                        .allowlisted_origins = {"isolated-app://*"},
                        .testing_url = kValidIsolatedAppId1,
                        .expected_is_get_all_screens_media_allowed = false,
                    }),
                    TestParam({
                        .allowlist_policy_name =
                            policy::key::kMultiScreenCaptureAllowedForUrls,
                        .allowlisted_origins = {"*"},
                        .testing_url = kValidIsolatedAppId1,
                        .expected_is_get_all_screens_media_allowed = false,
                    }),
                    TestParam({
                        .allowlist_policy_name =
                            policy::key::kMultiScreenCaptureAllowedForUrls,
                        .allowlisted_origins = {kValidIsolatedAppId1},
                        .testing_url = kValidIsolatedAppId1,
                        .expected_is_get_all_screens_media_allowed = true,
                    }),
                    TestParam({
                        .allowlist_policy_name =
                            policy::key::kMultiScreenCaptureAllowedForUrls,
                        .allowlisted_origins = {},
                        .testing_url = kValidIsolatedAppId1,
                        .expected_is_get_all_screens_media_allowed = false,
                    }),
                    TestParam({
                        .allowlist_policy_name =
                            policy::key::kMultiScreenCaptureAllowedForUrls,
                        .allowlisted_origins = {},
                        .testing_url = kValidIsolatedAppId2,
                        .expected_is_get_all_screens_media_allowed = false,
                    }),
                    TestParam({
                        .allowlist_policy_name =
                            policy::key::kMultiScreenCaptureAllowedForUrls,
                        .allowlisted_origins = {kValidIsolatedAppId1},
                        .testing_url = kValidIsolatedAppId1,
                        .expected_is_get_all_screens_media_allowed = true,
                    }),
                    TestParam({
                        .allowlist_policy_name =
                            policy::key::kMultiScreenCaptureAllowedForUrls,
                        .allowlisted_origins = {kValidIsolatedAppId1},
                        .testing_url = kValidIsolatedAppId2,
                        .expected_is_get_all_screens_media_allowed = false,
                    })));

class MultiCaptureDataDynamicRefreshTest
    : public MultiCaptureDataTestBase,
      public testing::WithParamInterface<NoRefreshTestParam> {
 public:
  MultiCaptureDataDynamicRefreshTest()
      : MultiCaptureDataTestBase(GetParam().allowlist_policy_name) {}
  ~MultiCaptureDataDynamicRefreshTest() override = default;

  explicit MultiCaptureDataDynamicRefreshTest(
      const MultiCaptureDataDynamicRefreshTest&) = delete;
  MultiCaptureDataDynamicRefreshTest& operator=(
      const MultiCaptureDataDynamicRefreshTest&) = delete;

  std::vector<std::string> GetAllowedOrigins() const override {
    return GetParam().original_allowlisted_origins;
  }

  void CheckExpectedAllowlistedAndForbiddenOrigins() {
    content::WebContents* current_web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    DCHECK(current_web_contents);
    for (const auto& expected_allowed_origin :
         GetParam().expected_allowed_origins) {
      EXPECT_TRUE(
          multi_capture::MultiCaptureDataServiceFactory::GetForBrowserContext(
              browser()->profile())
              ->IsMultiCaptureAllowed(GURL(expected_allowed_origin)));
    }

    for (const auto& expected_forbidden_origin :
         GetParam().expected_forbidden_origins) {
      EXPECT_FALSE(
          multi_capture::MultiCaptureDataServiceFactory::GetForBrowserContext(
              browser()->profile())
              ->IsMultiCaptureAllowed(GURL(expected_forbidden_origin)));
    }
  }
};

// This test checks that dynamic refresh (i.e. no origins can be added or
// removed after session start) is not happening.
IN_PROC_BROWSER_TEST_P(MultiCaptureDataDynamicRefreshTest,
                       EnabledWithMultipleAllowedOriginsNoDynamicRefresh) {
  CheckExpectedAllowlistedAndForbiddenOrigins();

  SetAllowedOriginsPolicy(GetParam().updated_allowlisted_origins);
  CheckExpectedAllowlistedAndForbiddenOrigins();
}

INSTANTIATE_TEST_SUITE_P(
    MultiCaptureDataDynamicRefreshTestWithParams,
    MultiCaptureDataDynamicRefreshTest,
    testing::Values(NoRefreshTestParam({
                        .allowlist_policy_name =
                            policy::key::kMultiScreenCaptureAllowedForUrls,
                        .original_allowlisted_origins = {kValidIsolatedAppId1},
                        .updated_allowlisted_origins = {kValidIsolatedAppId1,
                                                        kValidIsolatedAppId2},
                        .expected_allowed_origins = {kValidIsolatedAppId1},
                        .expected_forbidden_origins = {kValidIsolatedAppId2},
                    }),
                    NoRefreshTestParam({
                        .allowlist_policy_name =
                            policy::key::kMultiScreenCaptureAllowedForUrls,
                        .original_allowlisted_origins = {},
                        .updated_allowlisted_origins = {kValidIsolatedAppId1,
                                                        kValidIsolatedAppId2},
                        .expected_allowed_origins = {},
                        .expected_forbidden_origins = {kValidIsolatedAppId1,
                                                       kValidIsolatedAppId2},
                    })));
