// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/db/safebrowsing.pb.h"
#include "components/safe_browsing/db/util.h"
#include "components/safe_browsing/db/v4_embedded_test_server_util.h"
#include "components/safe_browsing/db/v4_test_util.h"
#include "components/safe_browsing/features.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool IsShowingInterstitial(content::WebContents* contents) {
  if (base::FeatureList::IsEnabled(safe_browsing::kCommittedSBInterstitials)) {
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            contents);
    return helper &&
           (helper
                ->GetBlockingPageForCurrentlyCommittedNavigationForTesting() !=
            nullptr);
  }
  return contents->GetInterstitialPage() != nullptr;
}

}  // namespace

namespace safe_browsing {

// This harness tests test-only code for correctness. This ensures that other
// test classes which want to use the V4 interceptor are testing the right
// thing.
class V4EmbeddedTestServerBrowserTest : public InProcessBrowserTest {
 public:
  V4EmbeddedTestServerBrowserTest() {}
  ~V4EmbeddedTestServerBrowserTest() override {}

  void SetUp() override {
    // We only need to mock a local database. The tests will use a true real V4
    // protocol manager.
    V4Database::RegisterStoreFactoryForTest(
        std::make_unique<TestV4StoreFactory>());

    auto v4_db_factory = std::make_unique<TestV4DatabaseFactory>();
    v4_db_factory_ = v4_db_factory.get();
    V4Database::RegisterDatabaseFactoryForTest(std::move(v4_db_factory));

    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }
  void TearDown() override {
    InProcessBrowserTest::TearDown();
    V4Database::RegisterStoreFactoryForTest(nullptr);
    V4Database::RegisterDatabaseFactoryForTest(nullptr);
  }

  // Only marks the prefix as bad in the local database. The server will respond
  // with the source of truth.
  void LocallyMarkPrefixAsBad(const GURL& url, const ListIdentifier& list_id) {
    FullHash full_hash = V4ProtocolManagerUtil::GetFullHash(url);
    v4_db_factory_->MarkPrefixAsBad(list_id, full_hash);
  }

 private:
  std::unique_ptr<net::MappedHostResolver> mapped_host_resolver_;

  // Owned by the V4Database.
  TestV4DatabaseFactory* v4_db_factory_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(V4EmbeddedTestServerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(V4EmbeddedTestServerBrowserTest, SimpleTest) {
  const char kMalwarePage[] = "/safe_browsing/malware.html";
  const GURL bad_url = embedded_test_server()->GetURL(kMalwarePage);

  ThreatMatch match;
  FullHash full_hash = V4ProtocolManagerUtil::GetFullHash(bad_url);
  LocallyMarkPrefixAsBad(bad_url, GetUrlMalwareId());
  match.set_platform_type(GetUrlMalwareId().platform_type());
  match.set_threat_entry_type(ThreatEntryType::URL);
  match.set_threat_type(ThreatType::MALWARE_THREAT);
  match.mutable_threat()->set_hash(full_hash);
  match.mutable_cache_duration()->set_seconds(300);

  std::map<GURL, safe_browsing::ThreatMatch> response_map{{bad_url, match}};
  StartRedirectingV4RequestsForTesting(response_map, embedded_test_server());
  embedded_test_server()->StartAcceptingConnections();

  ui_test_utils::NavigateToURL(browser(), bad_url);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsShowingInterstitial(contents));
}

IN_PROC_BROWSER_TEST_F(V4EmbeddedTestServerBrowserTest,
                       WrongFullHash_NoInterstitial) {
  const char kMalwarePage[] = "/safe_browsing/malware.html";
  const GURL bad_url = embedded_test_server()->GetURL(kMalwarePage);

  // Return a different full hash, so there will be no match and no
  // interstitial.
  ThreatMatch match;
  FullHash full_hash =
      V4ProtocolManagerUtil::GetFullHash(GURL("https://example.test/"));
  LocallyMarkPrefixAsBad(bad_url, GetUrlMalwareId());
  match.set_platform_type(GetUrlMalwareId().platform_type());
  match.set_threat_entry_type(ThreatEntryType::URL);
  match.set_threat_type(ThreatType::MALWARE_THREAT);
  match.mutable_threat()->set_hash(full_hash);
  match.mutable_cache_duration()->set_seconds(300);

  std::map<GURL, safe_browsing::ThreatMatch> response_map{{bad_url, match}};
  StartRedirectingV4RequestsForTesting(response_map, embedded_test_server());
  embedded_test_server()->StartAcceptingConnections();

  ui_test_utils::NavigateToURL(browser(), bad_url);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(IsShowingInterstitial(contents));
}

}  // namespace safe_browsing
