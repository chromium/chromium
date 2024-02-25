// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/signed_exchange_browser_test_helper.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "url/gurl.h"

namespace policy {

class SignedExchangePolicyTest : public PolicyTest {
 public:
  SignedExchangePolicyTest() = default;
  ~SignedExchangePolicyTest() override = default;

  void SetUp() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &SignedExchangePolicyTest::MonitorRequest, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    sxg_test_helper_.SetUp();
    PolicyTest::SetUp();
  }

  void TearDownOnMainThread() override {
    PolicyTest::TearDownOnMainThread();
    sxg_test_helper_.TearDownOnMainThread();
  }

 protected:
  void SetSignedExchangePolicy(bool enabled) {
    PolicyMap policies;
    policies.Set(key::kSignedHTTPExchangeEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(enabled),
                 nullptr);
    UpdateProviderPolicy(policies);
  }

  void InstallUrlInterceptor(const GURL& url, const std::string& data_path) {
    sxg_test_helper_.InstallUrlInterceptor(url, data_path);
  }

  bool HadSignedExchangeInAcceptHeader(const GURL& url) const {
    const auto it = url_accept_header_map_.find(url);
    if (it == url_accept_header_map_.end())
      return false;
    return it->second.find("application/signed-exchange") != std::string::npos;
  }

 private:
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    const auto it = request.headers.find("Accept");
    if (it == request.headers.end())
      return;
    url_accept_header_map_[request.base_url.Resolve(request.relative_url)] =
        it->second;
  }

  content::SignedExchangeBrowserTestHelper sxg_test_helper_;
  std::map<GURL, std::string> url_accept_header_map_;
};

IN_PROC_BROWSER_TEST_F(SignedExchangePolicyTest, SignedExchangeDisabled) {
  SetSignedExchangePolicy(false);

  content::DownloadTestObserverTerminal download_observer(
      browser()->profile()->GetDownloadManager(), 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_DENY);

  GURL url = embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  download_observer.WaitForFinished();

  // Check that the SXG file was not loaded as a page, but downloaded.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  browser()->profile()->GetDownloadManager()->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  EXPECT_EQ(downloads[0]->GetURL(), url);

  ASSERT_FALSE(HadSignedExchangeInAcceptHeader(url));
}

IN_PROC_BROWSER_TEST_F(SignedExchangePolicyTest, SignedExchangeEnabled) {
  SetSignedExchangePolicy(true);

  InstallUrlInterceptor(GURL("https://test.example.org/test/"),
                        "content/test/data/sxg/fallback.html");

  GURL url = embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");
  std::u16string title = u"Fallback URL response";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), title);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Check that the SXG file was handled as a Signed Exchange, and the
  // navigation was redirected to the SXG's fallback URL.
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());

  ASSERT_TRUE(HadSignedExchangeInAcceptHeader(url));
}

}  // namespace policy
