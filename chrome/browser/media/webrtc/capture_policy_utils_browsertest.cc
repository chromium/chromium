// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

// TODO(crbug.com/1447824): Implement no-dynamic refresh (and a test) for
// lacros.
namespace {
struct TestParam {
  std::vector<std::string> allow_listed_origins;
  std::string testing_url;
  bool expected_is_get_all_screens_media_allowed;
};

struct NoRefreshTestParam {
  std::vector<std::string> original_allowlisted_origins;
  std::vector<std::string> updated_allowlisted_origins;
  std::vector<std::string> expected_allowed_origins;
  std::vector<std::string> expected_forbidden_origins;
};

}  // namespace

class SelectAllScreensTestBase : public policy::PolicyTest {
 public:
  SelectAllScreensTestBase() = default;
  ~SelectAllScreensTestBase() override = default;

  SelectAllScreensTestBase(const SelectAllScreensTestBase&) = delete;
  SelectAllScreensTestBase& operator=(const SelectAllScreensTestBase&) = delete;

  void SetAllowedOriginsPolicy(
      const std::vector<std::string>& allow_listed_origins) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    policy::PolicyMap policies;
    base::Value::List allowed_origins;
    for (const auto& allowed_origin : allow_listed_origins) {
      allowed_origins.Append(base::Value(allowed_origin));
    }
    PolicyTest::SetPolicy(
        &policies,
        policy::key::kGetDisplayMediaSetSelectAllScreensAllowedForUrls,
        base::Value(std::move(allowed_origins)));
    provider_.UpdateChromePolicy(policies);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    SetAllowedOriginsPolicy(GetAllowedOrigins());
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  virtual std::vector<std::string> GetAllowedOrigins() const = 0;
};

class SelectAllScreensTest : public SelectAllScreensTestBase,
                             public testing::WithParamInterface<TestParam> {
 public:
  SelectAllScreensTest() = default;
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
  EXPECT_EQ(GetParam().expected_is_get_all_screens_media_allowed,
            capture_policy::IsGetAllScreensMediaAllowed(
                current_web_contents->GetBrowserContext(),
                GURL(GetParam().testing_url)));
}

INSTANTIATE_TEST_SUITE_P(
    SelectAllScreensTestWithParams,
    SelectAllScreensTest,
    testing::Values(
        TestParam({
            .allow_listed_origins = {""},
            .testing_url = "",
            .expected_is_get_all_screens_media_allowed = false,
        }),
        TestParam({
            .allow_listed_origins = {},
            .testing_url = "https://www.chromium.org",
            .expected_is_get_all_screens_media_allowed = false,
        })
#if BUILDFLAG(IS_CHROMEOS_ASH)
            ,
        TestParam({
            .allow_listed_origins = {},
            .testing_url = "",
            .expected_is_get_all_screens_media_allowed = false,
        }),
        TestParam({
            .allow_listed_origins = {"https://www.chromium.org"},
            .testing_url = "https://www.chromium.org",
            .expected_is_get_all_screens_media_allowed = true,
        }),
        TestParam({
            .allow_listed_origins = {"[*.]chromium.org"},
            .testing_url = "https://sub.chromium.org",
            .expected_is_get_all_screens_media_allowed = true,
        }),
        TestParam({
            .allow_listed_origins = {"[*.]chrome.org", "[*.]chromium.com"},
            .testing_url = "https://www.chromium.org",
            .expected_is_get_all_screens_media_allowed = false,
        }),
        TestParam({
            .allow_listed_origins = {"[*.]chrome.org", "[*.]chromium.org"},
            .testing_url = "https://www.chromium.org",
            .expected_is_get_all_screens_media_allowed = true,
        })
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
            ));

#if BUILDFLAG(IS_CHROMEOS_ASH)
class SelectAllScreensDynamicRefreshTest
    : public SelectAllScreensTestBase,
      public testing::WithParamInterface<NoRefreshTestParam> {
 public:
  SelectAllScreensDynamicRefreshTest() = default;
  ~SelectAllScreensDynamicRefreshTest() override = default;

  SelectAllScreensDynamicRefreshTest(const SelectAllScreensTest&) = delete;
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
      EXPECT_TRUE(capture_policy::IsGetAllScreensMediaAllowed(
          current_web_contents->GetBrowserContext(),
          GURL(expected_allowed_origin)));
    }

    for (const auto& expected_forbidden_origin :
         GetParam().expected_forbidden_origins) {
      EXPECT_FALSE(capture_policy::IsGetAllScreensMediaAllowed(
          current_web_contents->GetBrowserContext(),
          GURL(expected_forbidden_origin)));
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
    testing::Values(
        NoRefreshTestParam({
            .original_allowlisted_origins = {"https://www.chromium.org"},
            .updated_allowlisted_origins = {},
            .expected_allowed_origins = {"https://www.chromium.org"},
            .expected_forbidden_origins = {"https://www.chromium.com"},
        }),
        NoRefreshTestParam({
            .original_allowlisted_origins = {},
            .updated_allowlisted_origins = {"https://www.chromium.org"},
            .expected_allowed_origins = {},
            .expected_forbidden_origins = {},
        })));

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
