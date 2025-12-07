// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/quota/quota_features.h"
#include "storage/browser/quota/quota_manager_impl.h"
#include "third_party/blink/public/common/features.h"

namespace policy {

namespace {
const int64_t kGBytes = 1024 * 1024 * 1024;

const int64_t kDynamicQuotaForTestBrowser = 5 * 1024 * 1024;  // 5 MB
}  // namespace

class StaticStorageQuotaPolicyTest : public PolicyTest {
 public:
  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    base::FilePath test_data_dir;
    GetTestDataDirectory(&test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Navigates to an empty page.
  void NavigateToEmptyPage(Browser* browser) {
    CHECK(ui_test_utils::NavigateToURL(
        browser, embedded_test_server()->GetURL("/empty.html")));
  }

  int64_t GetEstimatedQuota(Browser* browser) {
    return content::EvalJs(
               browser->tab_strip_model()->GetActiveWebContents(),
               "(async () => { "
               "  const estimate = await navigator.storage.estimate(); "
               "  return estimate.quota; "
               "})()")
        .ExtractDouble();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

class StaticStorageQuotaPolicyFeatureEnabledTest
    : public StaticStorageQuotaPolicyTest {
 public:
  StaticStorageQuotaPolicyFeatureEnabledTest() {
    feature_list_.InitAndEnableFeature(storage::features::kStaticStorageQuota);
  }
};

class StaticStorageQuotaPolicyFeatureDisabledTest
    : public StaticStorageQuotaPolicyTest {
 public:
  StaticStorageQuotaPolicyFeatureDisabledTest() {
    feature_list_.InitAndDisableFeature(storage::features::kStaticStorageQuota);
  }
};

IN_PROC_BROWSER_TEST_F(StaticStorageQuotaPolicyFeatureDisabledTest,
                       StaticStorageQuotaEnabled) {
  // Verify dynamic quota behavior before the policy is set.
  NavigateToEmptyPage(browser());
  int64_t quota = GetEstimatedQuota(browser());
  EXPECT_EQ(quota, kDynamicQuotaForTestBrowser);

  // Set the policy to enable static storage quota.
  PolicyMap policies;
  SetPolicy(&policies, key::kStaticStorageQuotaEnabled, base::Value(true));
  UpdateProviderPolicy(policies);

  // StaticStorageQuotaEnabled doesn't support dynamic refresh, so we use an
  // incognito mode browser to test the policy.
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  NavigateToEmptyPage(incognito_browser);
  quota = GetEstimatedQuota(incognito_browser);
  // Expect reported quota to be exactly 1 GiB since usage is 0 and the
  // peculiarities of the test incognito_browser triggers the <1 GiB disk space
  // logic.
  EXPECT_EQ(quota, 1 * kGBytes);
}

IN_PROC_BROWSER_TEST_F(StaticStorageQuotaPolicyFeatureEnabledTest,
                       StaticStorageQuotaDisabled) {
  // Verify static quota behavior before the policy is set.
  NavigateToEmptyPage(browser());
  int64_t quota = GetEstimatedQuota(browser());
  // Expect reported quota to be exactly 10 GiB since usage is 0.
  EXPECT_EQ(quota, 10 * kGBytes);

  // Set the policy to disable static storage quota.
  PolicyMap policies;
  SetPolicy(&policies, key::kStaticStorageQuotaEnabled, base::Value(false));
  UpdateProviderPolicy(policies);

  // StaticStorageQuotaEnabled doesn't support dynamic refresh, so we use an
  // incognito mode browser to test the policy.
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  NavigateToEmptyPage(incognito_browser);
  quota = GetEstimatedQuota(incognito_browser);
  // Expect dynamic quota since the feature is disabled.
  EXPECT_EQ(quota, kDynamicQuotaForTestBrowser);
}

}  // namespace policy
