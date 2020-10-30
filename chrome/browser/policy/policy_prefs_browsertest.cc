// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/policy_pref_mapping_test.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/external_data_manager.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Return;

namespace policy {

namespace {

const char kCrosSettingsPrefix[] = "cros.";

}  // namespace

typedef InProcessBrowserTest PolicyPrefsTestCoverageTest;

IN_PROC_BROWSER_TEST_F(PolicyPrefsTestCoverageTest, AllPoliciesHaveATestCase) {
  base::FilePath test_case_path = ui_test_utils::GetTestFilePath(
      base::FilePath(FILE_PATH_LITERAL("policy")),
      base::FilePath(FILE_PATH_LITERAL("policy_test_cases.json")));
  VerifyAllPoliciesHaveATestCase(test_case_path);
}

// Base class for tests that change policy.
class PolicyPrefsTest : public InProcessBrowserTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetUpOnMainThread() override {
    search_test_utils::WaitForTemplateURLServiceToLoad(
        TemplateURLServiceFactory::GetForProfile(browser()->profile()));
  }

  void TearDownOnMainThread() override { ClearProviderPolicy(); }

  void ClearProviderPolicy() {
    provider_.UpdateChromePolicy(PolicyMap());
    base::RunLoop().RunUntilIdle();
  }

  MockConfigurationPolicyProvider provider_;
};

// Verifies that policies make their corresponding preferences become managed,
// and that the user can't override that setting.
IN_PROC_BROWSER_TEST_F(PolicyPrefsTest, PolicyToPrefsMapping) {
  base::FilePath test_case_path = ui_test_utils::GetTestFilePath(
      base::FilePath(FILE_PATH_LITERAL("policy")),
      base::FilePath(FILE_PATH_LITERAL("policy_test_cases.json")));
  PrefService* local_state = g_browser_process->local_state();
  PrefService* user_prefs = browser()->profile()->GetPrefs();

  VerifyPolicyToPrefMappings(test_case_path, local_state, user_prefs,
                             &provider_, kCrosSettingsPrefix);
}

// For WebUI integration tests, see cr_policy_indicator_tests.js and
// cr_policy_pref_indicator_tests.js.

}  // namespace policy
