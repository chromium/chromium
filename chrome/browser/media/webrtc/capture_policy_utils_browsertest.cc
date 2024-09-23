// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/capture_policy_utils.h"

#include <string>
#include <vector>

#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {
constexpr char kValidIsolatedAppId1[] =
    "isolated-app://pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic";
}  // namespace

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  Browser* current_browser = browser();
  TabStripModel* current_tab_strip_model = current_browser->tab_strip_model();
  content::WebContents* current_web_contents =
      current_tab_strip_model->GetWebContentsAt(0);

  base::test::TestFuture<bool> future;
  capture_policy::CheckGetAllScreensMediaAllowed(
      current_web_contents->GetBrowserContext(), GURL(GetParam().testing_url),
      future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(GetParam().expected_is_get_all_screens_media_allowed,
            future.Get<bool>());
}

INSTANTIATE_TEST_SUITE_P(
    DeprecatedPolicySelectAllScreensTestWithParams,
    SelectAllScreensTest,
    testing::Values(
        TestParam({
            .allow_list_policy_name =
                policy::key::kGetDisplayMediaSetSelectAllScreensAllowedForUrls,
            .allow_listed_origins = {""},
            .testing_url = "",
            .expected_is_get_all_screens_media_allowed = false,
        }),
        TestParam({
            .allow_list_policy_name =
                policy::key::kGetDisplayMediaSetSelectAllScreensAllowedForUrls,
            .allow_listed_origins = {},
            .testing_url = "https://www.chromium.org",
            .expected_is_get_all_screens_media_allowed = false,
        }),
        TestParam({
            .allow_list_policy_name =
                policy::key::kGetDisplayMediaSetSelectAllScreensAllowedForUrls,
            .allow_listed_origins = {},
            .testing_url = "",
            .expected_is_get_all_screens_media_allowed = false,
        }),
        TestParam({
            .allow_list_policy_name =
                policy::key::kGetDisplayMediaSetSelectAllScreensAllowedForUrls,
            .allow_listed_origins = {"https://www.chromium.org"},
            .testing_url = "https://www.chromium.org",
            .expected_is_get_all_screens_media_allowed = true,
        }),
        TestParam({
            .allow_list_policy_name =
                policy::key::kGetDisplayMediaSetSelectAllScreensAllowedForUrls,
            .allow_listed_origins = {"[*.]chromium.org"},
            .testing_url = "https://sub.chromium.org",
            .expected_is_get_all_screens_media_allowed = true,
        }),
        TestParam({
            .allow_list_policy_name =
                policy::key::kGetDisplayMediaSetSelectAllScreensAllowedForUrls,
            .allow_listed_origins = {"[*.]chrome.org", "[*.]chromium.com"},
            .testing_url = "https://www.chromium.org",
            .expected_is_get_all_screens_media_allowed = false,
        }),
        TestParam({
            .allow_list_policy_name =
                policy::key::kGetDisplayMediaSetSelectAllScreensAllowedForUrls,
            .allow_listed_origins = {"[*.]chrome.org", "[*.]chromium.org"},
            .testing_url = "https://www.chromium.org",
            .expected_is_get_all_screens_media_allowed = true,
        })));

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
          current_web_contents->GetBrowserContext(),
          GURL(expected_allowed_origin), future.GetCallback());
      ASSERT_TRUE(future.Wait());
      EXPECT_TRUE(future.Get<bool>());
    }

    for (const auto& expected_forbidden_origin :
         GetParam().expected_forbidden_origins) {
      base::test::TestFuture<bool> future;
      capture_policy::CheckGetAllScreensMediaAllowed(
          current_web_contents->GetBrowserContext(),
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
    DeprecatedPolicySelectAllScreensDynamicRefreshTestWithParams,
    SelectAllScreensDynamicRefreshTest,
    testing::Values(
        NoRefreshTestParam({
            .allow_list_policy_name =
                policy::key::kGetDisplayMediaSetSelectAllScreensAllowedForUrls,
            .original_allowlisted_origins = {"https://www.chromium.org"},
            .updated_allowlisted_origins = {},
            .expected_allowed_origins = {"https://www.chromium.org"},
            .expected_forbidden_origins = {"https://www.chromium.com"},
        }),
        NoRefreshTestParam({
            .allow_list_policy_name =
                policy::key::kGetDisplayMediaSetSelectAllScreensAllowedForUrls,
            .original_allowlisted_origins = {},
            .updated_allowlisted_origins = {"https://www.chromium.org"},
            .expected_allowed_origins = {},
            .expected_forbidden_origins = {},
        })));

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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using CaptureUtilsBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(CaptureUtilsBrowserTest, MultiCaptureServiceExists) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (chromeos::LacrosService::Get()
          ->GetInterfaceVersion<crosapi::mojom::MultiCaptureService>() <
      static_cast<int>(crosapi::mojom::MultiCaptureService::MethodMinVersions::
                           kIsMultiCaptureAllowedMinVersion)) {
    GTEST_SKIP();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  EXPECT_TRUE(capture_policy::GetMultiCaptureService());
}

IN_PROC_BROWSER_TEST_F(CaptureUtilsBrowserTest,
                       NoPolicySetMultiCaptureServiceMaybeExists) {
  Browser* current_browser = browser();
  TabStripModel* current_tab_strip_model = current_browser->tab_strip_model();
  content::WebContents* current_web_contents =
      current_tab_strip_model->GetWebContentsAt(0);

  base::test::TestFuture<bool> future;
  capture_policy::CheckGetAllScreensMediaAllowed(
      current_web_contents->GetBrowserContext(), GURL(kValidIsolatedAppId1),
      future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get<bool>());
}
