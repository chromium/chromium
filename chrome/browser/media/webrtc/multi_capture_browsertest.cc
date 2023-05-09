// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

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

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_CHROMEOS_ASH)
#define MULTI_CAPTURE_SUPPORTED_ON_PLATFORM true
#else
#define MULTI_CAPTURE_SUPPORTED_ON_PLATFORM false
#endif

class SelectAllScreensTest : public InProcessBrowserTest {
 public:
  SelectAllScreensTest() = default;
  ~SelectAllScreensTest() override = default;

  SelectAllScreensTest(const SelectAllScreensTest&) = delete;
  SelectAllScreensTest& operator=(const SelectAllScreensTest&) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

IN_PROC_BROWSER_TEST_F(SelectAllScreensTest,
                       SelectAllScreensDisabledByDefault) {
  Browser* current_browser = browser();
  TabStripModel* current_tab_strip_model = current_browser->tab_strip_model();
  content::WebContents* current_web_contents =
      current_tab_strip_model->GetWebContentsAt(0);
  EXPECT_FALSE(capture_policy::IsGetAllScreensMediaAllowed(
      current_web_contents->GetBrowserContext(), GURL("")));
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SelectAllScreensTest,
                       SelectAllScreensDisabledWithEmptyPolicy) {
  policy::PolicyMap policies;
  policies.Set(policy::key::kGetDisplayMediaSetSelectAllScreensAllowedForUrls,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
               base::Value(base::Value::List()), nullptr);
  provider_.UpdateChromePolicy(policies);

  Browser* current_browser = browser();
  TabStripModel* current_tab_strip_model = current_browser->tab_strip_model();
  content::WebContents* current_web_contents =
      current_tab_strip_model->GetWebContentsAt(0);
  EXPECT_FALSE(capture_policy::IsGetAllScreensMediaAllowed(
      current_web_contents->GetBrowserContext(), GURL("")));
}

IN_PROC_BROWSER_TEST_F(SelectAllScreensTest,
                       SelectAllScreensEnabledWithCorrectUrl) {
  policy::PolicyMap policies;
  base::Value::List allowed_origins;
  allowed_origins.Append(base::Value("https://www.chromium.org"));
  policies.Set(policy::key::kGetDisplayMediaSetSelectAllScreensAllowedForUrls,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
               base::Value(std::move(allowed_origins)), nullptr);
  provider_.UpdateChromePolicy(policies);

  base::RunLoop().RunUntilIdle();

  Browser* current_browser = browser();
  TabStripModel* current_tab_strip_model = current_browser->tab_strip_model();
  content::WebContents* current_web_contents =
      current_tab_strip_model->GetWebContentsAt(0);
  const bool multi_capture_allowed =
      capture_policy::IsGetAllScreensMediaAllowed(
          current_web_contents->GetBrowserContext(),
          GURL("https://www.chromium.org"));
  EXPECT_EQ(multi_capture_allowed, MULTI_CAPTURE_SUPPORTED_ON_PLATFORM);
}

IN_PROC_BROWSER_TEST_F(SelectAllScreensTest,
                       SelectAllScreensEnabledWithCorrectUrlWildcard) {
  policy::PolicyMap policies;
  base::Value::List allowed_origins;
  allowed_origins.Append(base::Value("[*.]chromium.org"));
  policies.Set(policy::key::kGetDisplayMediaSetSelectAllScreensAllowedForUrls,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
               base::Value(std::move(allowed_origins)), nullptr);
  provider_.UpdateChromePolicy(policies);

  base::RunLoop().RunUntilIdle();

  Browser* current_browser = browser();
  TabStripModel* current_tab_strip_model = current_browser->tab_strip_model();
  content::WebContents* current_web_contents =
      current_tab_strip_model->GetWebContentsAt(0);
  const bool multi_capture_allowed =
      capture_policy::IsGetAllScreensMediaAllowed(
          current_web_contents->GetBrowserContext(),
          GURL("https://sub.chromium.org"));
  EXPECT_EQ(multi_capture_allowed, MULTI_CAPTURE_SUPPORTED_ON_PLATFORM);
}

IN_PROC_BROWSER_TEST_F(SelectAllScreensTest,
                       SelectAllScreensDisabledWithWrongUrlWildCard) {
  policy::PolicyMap policies;
  base::Value::List allowed_origins;
  allowed_origins.Append(base::Value("[*.]chrome.org"));
  allowed_origins.Append(base::Value("[*.]chromium.com"));
  policies.Set(policy::key::kGetDisplayMediaSetSelectAllScreensAllowedForUrls,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
               base::Value(std::move(allowed_origins)), nullptr);
  provider_.UpdateChromePolicy(policies);

  base::RunLoop().RunUntilIdle();

  Browser* current_browser = browser();
  TabStripModel* current_tab_strip_model = current_browser->tab_strip_model();
  content::WebContents* current_web_contents =
      current_tab_strip_model->GetWebContentsAt(0);
  EXPECT_FALSE(capture_policy::IsGetAllScreensMediaAllowed(
      current_web_contents->GetBrowserContext(),
      GURL("https://www.chromium.org")));
}

IN_PROC_BROWSER_TEST_F(SelectAllScreensTest,
                       SelectAllScreensEnabledWithMultipleAllowedOrigins) {
  policy::PolicyMap policies;
  base::Value::List allowed_origins;
  allowed_origins.Append(base::Value("[*.]chrome.org"));
  allowed_origins.Append(base::Value("[*.]chromium.org"));
  policies.Set(policy::key::kGetDisplayMediaSetSelectAllScreensAllowedForUrls,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
               base::Value(std::move(allowed_origins)), nullptr);
  provider_.UpdateChromePolicy(policies);

  base::RunLoop().RunUntilIdle();

  Browser* current_browser = browser();
  TabStripModel* current_tab_strip_model = current_browser->tab_strip_model();
  content::WebContents* current_web_contents =
      current_tab_strip_model->GetWebContentsAt(0);
  const bool multi_capture_allowed =
      capture_policy::IsGetAllScreensMediaAllowed(
          current_web_contents->GetBrowserContext(),
          GURL("https://www.chromium.org"));
  EXPECT_EQ(multi_capture_allowed, MULTI_CAPTURE_SUPPORTED_ON_PLATFORM);
}

IN_PROC_BROWSER_TEST_F(
    SelectAllScreensTest,
    SelectAllScreensEnabledWithMultipleAllowedOriginsDynamicRefresh) {
  policy::PolicyMap policies;
  base::Value::List allowed_origins;
  allowed_origins.Append(base::Value("[*.]chrome.org"));
  policies.Set(policy::key::kGetDisplayMediaSetSelectAllScreensAllowedForUrls,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
               base::Value(std::move(allowed_origins)), nullptr);
  provider_.UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();

  Browser* current_browser = browser();
  TabStripModel* current_tab_strip_model = current_browser->tab_strip_model();
  content::WebContents* current_web_contents =
      current_tab_strip_model->GetWebContentsAt(0);

  bool multi_capture_allowed = capture_policy::IsGetAllScreensMediaAllowed(
      current_web_contents->GetBrowserContext(),
      GURL("https://www.chromium.org"));
  EXPECT_FALSE(multi_capture_allowed);

  policies.Clear();
  base::Value::List new_allowed_origins;
  new_allowed_origins.Append(base::Value("[*.]chrome.org"));
  new_allowed_origins.Append(base::Value("[*.]chromium.org"));
  policies.Set(policy::key::kGetDisplayMediaSetSelectAllScreensAllowedForUrls,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
               base::Value(std::move(new_allowed_origins)), nullptr);
  provider_.UpdateChromePolicy(policies);

  base::RunLoop().RunUntilIdle();

  multi_capture_allowed = capture_policy::IsGetAllScreensMediaAllowed(
      current_web_contents->GetBrowserContext(),
      GURL("https://www.chromium.org"));
  EXPECT_EQ(multi_capture_allowed, MULTI_CAPTURE_SUPPORTED_ON_PLATFORM);
}
#endif  // BUILDFLAG(IS_CHROMEOS)
