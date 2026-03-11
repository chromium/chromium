// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

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

const int64_t kMinBucketStaticQuota = 1 * kGBytes;       // 1 GB
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

// Tests for kIncognitoStaticStorageQuota feature which ensures consistent
// quota reporting in Incognito mode to prevent Incognito detection.
// This flag is expected to be merged into kStaticStorageQuota once it reaches
// 100% stable (see b/491017282).
class IncognitoStaticStorageQuotaEnabledTest
    : public StaticStorageQuotaPolicyTest {
 public:
  IncognitoStaticStorageQuotaEnabledTest() {
    feature_list_.InitWithFeatureStates(
        {{storage::features::kIncognitoStaticStorageQuota, true},
         {storage::features::kStaticStorageQuota, true}});
  }
};

class IncognitoStaticStorageQuotaDisabledTest
    : public StaticStorageQuotaPolicyTest {
 public:
  IncognitoStaticStorageQuotaDisabledTest() {
    feature_list_.InitWithFeatureStates(
        {{storage::features::kIncognitoStaticStorageQuota, false},
         {storage::features::kStaticStorageQuota, true}});
  }
};

IN_PROC_BROWSER_TEST_F(StaticStorageQuotaFeatureEnabledTest, RegularSession) {
  NavigateToEmptyPage(browser());
  // Expect reported quota to be exactly 10 GiB.
  EXPECT_EQ(GetEstimatedQuota(browser()), kDefaultBucketStaticQuota);
}

IN_PROC_BROWSER_TEST_F(StaticStorageQuotaFeatureDisabledTest, RegularSession) {
  NavigateToEmptyPage(browser());
  EXPECT_EQ(GetEstimatedQuota(browser()), kDynamicQuotaForTestBrowser);
}

IN_PROC_BROWSER_TEST_F(IncognitoStaticStorageQuotaEnabledTest,
                       IncognitoSession) {
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  NavigateToEmptyPage(incognito_browser);
  // Expect reported quota to be exactly 10 GiB in Incognito mode with the
  // kIncognitoStaticStorageQuota feature enabled.
  EXPECT_EQ(GetEstimatedQuota(incognito_browser), kDefaultBucketStaticQuota);
}

IN_PROC_BROWSER_TEST_F(IncognitoStaticStorageQuotaDisabledTest,
                       IncognitoSession) {
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  NavigateToEmptyPage(incognito_browser);
  // Expect quota in Incognito to be trimmed to 1 GiB, minimal value for the
  // static quota enabled.
  EXPECT_EQ(GetEstimatedQuota(incognito_browser), kMinBucketStaticQuota);
}

}  // namespace policy
