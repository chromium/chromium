// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/storage_partition.h"
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

const int64_t kDefaultBucketStaticQuota = 10 * kGBytes;  // 10 GB
}  // namespace

class StaticStorageQuotaPolicyTest : public PolicyTest {
 public:
  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    base::FilePath test_data_dir;
    GetTestDataDirectory(&test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->Start());

    static storage::QuotaSettings quota_settings(
        storage::GetHardCodedSettings(kDynamicQuotaForTestBrowser));
    content::StoragePartition::SetDefaultQuotaSettingsForTesting(
        &quota_settings);
  }

  // Navigates to an empty page.
  void NavigateToEmptyPage(BrowserWindowInterface* browser) {
    CHECK(ui_test_utils::NavigateToURL(
        browser, embedded_test_server()->GetURL("/empty.html")));
  }

  int64_t GetEstimatedQuota(BrowserWindowInterface* browser) {
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

class StaticStorageQuotaFeatureEnabledTest
    : public StaticStorageQuotaPolicyTest {
 public:
  StaticStorageQuotaFeatureEnabledTest() {
    feature_list_.InitAndEnableFeature(storage::features::kStaticStorageQuota);
  }
};

class StaticStorageQuotaFeatureDisabledTest
    : public StaticStorageQuotaPolicyTest {
 public:
  StaticStorageQuotaFeatureDisabledTest() {
    feature_list_.InitAndDisableFeature(storage::features::kStaticStorageQuota);
  }
};

IN_PROC_BROWSER_TEST_F(StaticStorageQuotaFeatureEnabledTest, RegularSession) {
  NavigateToEmptyPage(browser());
  // Expect reported quota to be exactly 10 GiB.
  EXPECT_EQ(GetEstimatedQuota(browser()), kDefaultBucketStaticQuota);
}

IN_PROC_BROWSER_TEST_F(StaticStorageQuotaFeatureEnabledTest, IncognitoSession) {
  BrowserWindowInterface* incognito_browser =
      OpenURLOffTheRecord(browser()->GetProfile(), GURL("about:blank"));
  NavigateToEmptyPage(incognito_browser);
  // Expect reported quota to be exactly 10 GiB in Incognito mode.
  EXPECT_EQ(GetEstimatedQuota(incognito_browser), kDefaultBucketStaticQuota);
}

IN_PROC_BROWSER_TEST_F(StaticStorageQuotaFeatureDisabledTest, RegularSession) {
  NavigateToEmptyPage(browser());
  EXPECT_EQ(GetEstimatedQuota(browser()), kDynamicQuotaForTestBrowser);
}

}  // namespace policy
