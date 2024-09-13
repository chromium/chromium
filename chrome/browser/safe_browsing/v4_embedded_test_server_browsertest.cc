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
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/core/browser/db/safebrowsing.pb.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_embedded_test_server_util.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
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
// test classes which want to use the V4 interceptor are testing the right
// thing.
class V4EmbeddedTestServerBrowserTest : public InProcessBrowserTest {
 public:
  V4EmbeddedTestServerBrowserTest() {}

  V4EmbeddedTestServerBrowserTest(const V4EmbeddedTestServerBrowserTest&) =
      delete;
  V4EmbeddedTestServerBrowserTest& operator=(
      const V4EmbeddedTestServerBrowserTest&) = delete;

  ~V4EmbeddedTestServerBrowserTest() override {}

  void SetUp() override {
    // We only need to mock a local database. The tests will use a true real V4
    // protocol manager.
    V4Database::RegisterStoreFactoryForTest(
        std::make_unique<TestV4StoreFactory>());

    auto v4_db_factory = std::make_unique<TestV4DatabaseFactory>();
    v4_db_factory_ = v4_db_factory.get();
    V4Database::RegisterDatabaseFactoryForTest(std::move(v4_db_factory));

    secure_embedded_test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::Type::TYPE_HTTPS);

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
    FullHashStr full_hash = V4ProtocolManagerUtil::GetFullHash(url);
    while (!v4_db_factory_->IsReady()) {
      content::RunAllTasksUntilIdle();
    }
    v4_db_factory_->MarkPrefixAsBad(list_id, full_hash);
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> secure_embedded_test_server_;

 private:
  std::unique_ptr<net::MappedHostResolver> mapped_host_resolver_;

  // Owned by the V4Database.
  raw_ptr<TestV4DatabaseFactory, AcrossTasksDanglingUntriaged> v4_db_factory_ =
      nullptr;
};

IN_PROC_BROWSER_TEST_F(V4EmbeddedTestServerBrowserTest, SimpleTest) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

  const char kMalwarePage[] = "/safe_browsing/malware.html";
  const GURL bad_url = embedded_test_server()->GetURL(kMalwarePage);

  ThreatMatch match;
  FullHashStr full_hash = V4ProtocolManagerUtil::GetFullHash(bad_url);
  LocallyMarkPrefixAsBad(bad_url, GetUrlMalwareId());
  match.set_platform_type(GetUrlMalwareId().platform_type());
  match.set_threat_entry_type(ThreatEntryType::URL);
  match.set_threat_type(ThreatType::MALWARE_THREAT);
  match.mutable_threat()->set_hash(full_hash);
  match.mutable_cache_duration()->set_seconds(300);

  std::map<GURL, safe_browsing::ThreatMatch> response_map{{bad_url, match}};
  StartRedirectingV4RequestsForTesting(response_map, embedded_test_server());
  embedded_test_server()->StartAcceptingConnections();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), bad_url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(contents));
}

IN_PROC_BROWSER_TEST_F(V4EmbeddedTestServerBrowserTest,
                       WrongFullHash_NoInterstitial) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

  const char kMalwarePage[] = "/safe_browsing/malware.html";
  const GURL bad_url = embedded_test_server()->GetURL(kMalwarePage);

  // Return a different full hash, so there will be no match and no
  // interstitial.
  ThreatMatch match;
  FullHashStr full_hash =
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), bad_url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(contents));
}

IN_PROC_BROWSER_TEST_F(V4EmbeddedTestServerBrowserTest, DoesNotSaveCookies) {
  ASSERT_TRUE(secure_embedded_test_server_->InitializeAndListen());
  const char kMalwarePage[] = "/safe_browsing/malware.html";
  const GURL bad_url = secure_embedded_test_server_->GetURL(kMalwarePage);

  ThreatMatch match;
  FullHashStr full_hash = V4ProtocolManagerUtil::GetFullHash(bad_url);
  LocallyMarkPrefixAsBad(bad_url, GetUrlMalwareId());
  match.set_platform_type(GetUrlMalwareId().platform_type());
  match.set_threat_entry_type(ThreatEntryType::URL);
  match.set_threat_type(ThreatType::MALWARE_THREAT);
  match.mutable_threat()->set_hash(full_hash);
  match.mutable_cache_duration()->set_seconds(0);

  std::map<GURL, safe_browsing::ThreatMatch> response_map{{bad_url, match}};
  StartRedirectingV4RequestsForTesting(
      response_map, secure_embedded_test_server_.get(),
      /*delay_map=*/std::map<GURL, base::TimeDelta>(),
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

}  // namespace safe_browsing
