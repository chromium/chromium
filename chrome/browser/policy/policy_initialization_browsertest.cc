// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#endif

namespace {

#if BUILDFLAG(IS_WIN)
const char kMockPolicyName[] = "AllowFileSelectionDialogs";
#endif

void VerifyLocalState() {
  const PrefService* prefs = g_browser_process->local_state();
  ASSERT_NE(nullptr, prefs);
  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kAllowFileSelectionDialogs);
  ASSERT_NE(nullptr, pref);
  EXPECT_FALSE(pref->IsDefaultValue());
}

class ChromeBrowserMainExtraPartsPolicyValueChecker
    : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsPolicyValueChecker() = default;
  ChromeBrowserMainExtraPartsPolicyValueChecker(
      const ChromeBrowserMainExtraPartsPolicyValueChecker&) = delete;
  ChromeBrowserMainExtraPartsPolicyValueChecker& operator=(
      const ChromeBrowserMainExtraPartsPolicyValueChecker&) = delete;

  // ChromeBrowserMainExtraParts
  void PreCreateThreads() override { VerifyLocalState(); }
  void PreBrowserStart() override { VerifyLocalState(); }
  void PreMainMessageLoopRun() override { VerifyLocalState(); }
};

}  // namespace

// Test if the policy value can be read from the pref properly on Windows.
class PolicyInitializationBrowserTest : public InProcessBrowserTest {
 public:
  PolicyInitializationBrowserTest(const PolicyInitializationBrowserTest&) =
      delete;
  PolicyInitializationBrowserTest& operator=(
      const PolicyInitializationBrowserTest&) = delete;

 protected:
  PolicyInitializationBrowserTest() = default;

  // content::BrowserTestBase:
  void SetUpInProcessBrowserTestFixture() override {
    SetUpPlatformPolicyValue();
  }
  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    InProcessBrowserTest::CreatedBrowserMainParts(parts);
    static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
        std::make_unique<ChromeBrowserMainExtraPartsPolicyValueChecker>());
  }

 private:
#if BUILDFLAG(IS_WIN)
  // Set up policy value for windows platform
  void SetUpPlatformPolicyValue() {
    HKEY root = HKEY_CURRENT_USER;
    ASSERT_NO_FATAL_FAILURE(registry_override_manager_.OverrideRegistry(root));

    base::win::RegKey key;

    ASSERT_EQ(ERROR_SUCCESS, key.Create(root, policy::kRegistryChromePolicyKey,
                                        KEY_SET_VALUE | KEY_WOW64_32KEY));
    ASSERT_EQ(ERROR_SUCCESS,
              key.WriteValue(base::ASCIIToWide(kMockPolicyName).c_str(), 1));
  }

  registry_util::RegistryOverrideManager registry_override_manager_;
#else
  // This test hasn't supported other platform yet.
  void SetUpPlatformPolicyValue() {}
#endif
};

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(PolicyInitializationBrowserTest, VerifyLocalState) {
  VerifyLocalState();
}
#endif
