// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/cookie_encryption_provider_impl.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

enum TestConfiguration {
  // Network Service is using Sync os_crypt API.
  kOSCryptSync,
  // Network Service is using Async API, i.e. SetCookieEncryptionProvider is
  // being called from SystemNetworkContextManager::OnNetworkServiceCreated
  // before creating any network contexts.
  kOSCryptAsync,
};

struct TestCase {
  std::string name;
  TestConfiguration before;
  TestConfiguration after;
};

}  // namespace

class CookieEncryptionProviderBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<TestCase> {
 public:
  void SetUp() override {
    auto configuration =
        content::IsPreTest() ? GetParam().before : GetParam().after;

    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    switch (configuration) {
      case kOSCryptSync:
        disabled_features.push_back(
            features::kUseOsCryptAsyncForCookieEncryption);
        break;
      case kOSCryptAsync:
        enabled_features.push_back(
            features::kUseOsCryptAsyncForCookieEncryption);
        break;
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(CookieEncryptionProviderBrowserTest, PRE_CookieStorage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/setcookie.html")));
}

IN_PROC_BROWSER_TEST_P(CookieEncryptionProviderBrowserTest, CookieStorage) {
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->GetCookieManager(cookie_manager.BindNewPipeAndPassReceiver());
  base::test::TestFuture<const net::CookieList&> future;
  cookie_manager->GetAllCookies(future.GetCallback());
  auto cookies = future.Take();
  ASSERT_EQ(cookies.size(), 1u);
  EXPECT_EQ(cookies[0].Name(), "name");
  EXPECT_EQ(cookies[0].Value(), "Good");
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    CookieEncryptionProviderBrowserTest,
    testing::ValuesIn<TestCase>({
        {.name = "sync", .before = kOSCryptSync, .after = kOSCryptSync},
        {.name = "async", .before = kOSCryptAsync, .after = kOSCryptAsync},
        {.name = "migration_sync_to_async",
         .before = kOSCryptSync,
         .after = kOSCryptAsync},
        {.name = "rollback_async_to_sync",
         .before = kOSCryptAsync,
         .after = kOSCryptSync},
    }),
    [](const auto& info) { return info.param.name; });
