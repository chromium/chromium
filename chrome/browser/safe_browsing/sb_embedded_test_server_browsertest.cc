// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/core/browser/db/safebrowsing.pb.h"
#include "components/safe_browsing/core/browser/db/sb_database.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_embedded_test_server_util.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#include "components/safe_browsing/core/browser/db/v5_embedded_test_server_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/cookies/canonical_cookie.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

std::vector<net::CanonicalCookie> GetCookies(
    network::mojom::NetworkContext* network_context) {
  base::RunLoop run_loop;
  std::vector<net::CanonicalCookie> cookies;
  mojo::Remote<network::mojom::CookieManager> cookie_manager_remote;
  network_context->GetCookieManager(
      cookie_manager_remote.BindNewPipeAndPassReceiver());
  cookie_manager_remote->GetAllCookies(base::BindOnce(
      [](base::RunLoop* run_loop,
         std::vector<net::CanonicalCookie>* out_cookies,
         const std::vector<net::CanonicalCookie>& cookies) {
        *out_cookies = cookies;
        run_loop->Quit();
      },
      &run_loop, &cookies));
  run_loop.Run();
  return cookies;
}

}  // namespace

namespace safe_browsing {

// This harness tests test-only code for correctness. This ensures that other
// test classes which want to use the V4/V5 interceptor are testing the right
// thing.
class SBEmbeddedTestServerBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  SBEmbeddedTestServerBrowserTest() {
    if (IsV5()) {
      feature_list_.InitAndEnableFeature(kLocalListsUseSBv5);
    } else {
      feature_list_.InitAndDisableFeature(kLocalListsUseSBv5);
    }
  }

  SBEmbeddedTestServerBrowserTest(const SBEmbeddedTestServerBrowserTest&) =
      delete;
  SBEmbeddedTestServerBrowserTest& operator=(
      const SBEmbeddedTestServerBrowserTest&) = delete;

  ~SBEmbeddedTestServerBrowserTest() override = default;

  bool IsV5() const { return GetParam(); }

  void SetUp() override {
    // We only need to mock a local database. The tests will use a true real
    // protocol manager.
    SBDatabase::RegisterStoreFactoryForTest(
        std::make_unique<TestV4StoreFactory>());

    auto sb_db_factory = std::make_unique<TestSBDatabaseFactory>();
    sb_db_factory_ = sb_db_factory.get();
    SBDatabase::RegisterDatabaseFactoryForTest(std::move(sb_db_factory));

    secure_embedded_test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::Type::TYPE_HTTPS);

    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    SBDatabase::RegisterStoreFactoryForTest(nullptr);
    SBDatabase::RegisterDatabaseFactoryForTest(nullptr);
  }

  // Only marks the prefix as bad in the local database. The server will respond
  // with the source of truth.
  void LocallyMarkPrefixAsBad(const GURL& url, const ListIdentifier& list_id) {
    FullHashStr full_hash = SBProtocolManagerUtil::GetFullHash(url);
    while (!sb_db_factory_->IsReady()) {
      content::RunAllTasksUntilIdle();
    }
    sb_db_factory_->MarkPrefixAsBad(list_id, full_hash);
  }

  void StartRedirectingRequests(const GURL& request_url,
                                const GURL& match_url,
                                net::EmbeddedTestServer* test_server,
                                bool serve_cookies) {
    if (IsV5()) {
      V5::FullHash match;
      match.set_full_hash(SBProtocolManagerUtil::GetFullHash(match_url));
      match.add_full_hash_details()->set_threat_type(V5::ThreatType::MALWARE);
      std::map<GURL, V5::FullHash> response_map{{request_url, match}};
      StartRedirectingV5RequestsForTesting(response_map, test_server,
                                           /*delay_map=*/{}, serve_cookies);
    } else {
      ThreatMatch match;
      FullHashStr full_hash = SBProtocolManagerUtil::GetFullHash(match_url);
      match.set_platform_type(GetUrlMalwareId().platform_type());
      match.set_threat_entry_type(ThreatEntryType::URL);
      match.set_threat_type(ThreatType::MALWARE_THREAT);
      match.mutable_threat()->set_hash(full_hash);
      match.mutable_cache_duration()->set_seconds(serve_cookies ? 0 : 300);

      std::map<GURL, safe_browsing::ThreatMatch> response_map{
          {request_url, match}};
      StartRedirectingV4RequestsForTesting(response_map, test_server,
                                           /*delay_map=*/{}, serve_cookies);
    }
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> secure_embedded_test_server_;

 private:
  // Owned by the SBDatabase.
  raw_ptr<TestSBDatabaseFactory, AcrossTasksDanglingUntriaged> sb_db_factory_ =
      nullptr;

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(SBEmbeddedTestServerBrowserTest, SimpleTest) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

  const char kMalwarePage[] = "/safe_browsing/malware.html";
  const GURL bad_url = embedded_test_server()->GetURL(kMalwarePage);

  LocallyMarkPrefixAsBad(bad_url, GetUrlMalwareId());
  StartRedirectingRequests(bad_url, bad_url, embedded_test_server(),
                           /*serve_cookies=*/false);
  embedded_test_server()->StartAcceptingConnections();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), bad_url));
  content::WebContents* contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(contents));
}

IN_PROC_BROWSER_TEST_P(SBEmbeddedTestServerBrowserTest,
                       WrongFullHash_NoInterstitial) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

  const char kMalwarePage[] = "/safe_browsing/malware.html";
  const GURL bad_url = embedded_test_server()->GetURL(kMalwarePage);

  // Return a different full hash, so there will be no match and no
  // interstitial.
  LocallyMarkPrefixAsBad(bad_url, GetUrlMalwareId());
  StartRedirectingRequests(bad_url, GURL("https://example.test/"),
                           embedded_test_server(),
                           /*serve_cookies=*/false);
  embedded_test_server()->StartAcceptingConnections();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), bad_url));
  content::WebContents* contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));
}

IN_PROC_BROWSER_TEST_P(SBEmbeddedTestServerBrowserTest, DoesNotSaveCookies) {
  ASSERT_TRUE(secure_embedded_test_server_->InitializeAndListen());
  const char kMalwarePage[] = "/safe_browsing/malware.html";
  const GURL bad_url = secure_embedded_test_server_->GetURL(kMalwarePage);

  LocallyMarkPrefixAsBad(bad_url, GetUrlMalwareId());
  StartRedirectingRequests(bad_url, bad_url, secure_embedded_test_server_.get(),
                           /*serve_cookies=*/true);
  secure_embedded_test_server_->StartAcceptingConnections();

  EXPECT_EQ(
      GetCookies(
          g_browser_process->system_network_context_manager()->GetContext())
          .size(),
      0u);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), bad_url));

  EXPECT_EQ(
      GetCookies(
          g_browser_process->system_network_context_manager()->GetContext())
          .size(),
      0u);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SBEmbeddedTestServerBrowserTest,
                         ::testing::Bool());

}  // namespace safe_browsing
