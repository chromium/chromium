// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/host_port_pair.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cors.mojom.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const auto kAllowSubdomains =
    network::mojom::CorsDomainMatchMode::kAllowSubdomains;
const auto kDisallowSubdomains =
    network::mojom::CorsDomainMatchMode::kDisallowSubdomains;

const char kTestPath[] = "/loader/cors_origin_access_list_test.html";

const char kTestHost[] = "crossorigin.example.com";
const char kTestHostInDifferentCase[] = "CrossOrigin.example.com";
const char kTestSubdomainHost[] = "subdomain.crossorigin.example.com";

// Tests end to end functionality of CORS access origin allow lists.
class CorsOriginAccessListBrowserTest : public InProcessBrowserTest {
 protected:
  CorsOriginAccessListBrowserTest() {
    // This test verifies if the CorsOriginAccessList works with OOR-CORS.
    scoped_feature_list_.InitAndEnableFeature(
        network::features::kOutOfBlinkCors);
  }
  std::unique_ptr<content::TitleWatcher> CreateWatcher() {
    // Register all possible result strings here.
    std::unique_ptr<content::TitleWatcher> watcher =
        std::make_unique<content::TitleWatcher>(web_contents(), pass_string());
    watcher->AlsoWaitForTitle(fail_string());
    return watcher;
  }

  std::string GetReason() {
    bool executing = true;
    std::string reason;
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        script_, base::BindOnce(
                     [](bool* flag, std::string* reason, base::Value value) {
                       *flag = false;
                       DCHECK(value.is_string());
                       *reason = value.GetString();
                     },
                     base::Unretained(&executing), base::Unretained(&reason)));
    while (executing) {
      base::RunLoop loop;
      loop.RunUntilIdle();
    }
    return reason;
  }

  void SetAllowList(const std::string& scheme,
                    const std::string& host,
                    network::mojom::CorsDomainMatchMode mode) {
    {
      std::vector<network::mojom::CorsOriginPatternPtr> list;
      list.push_back(network::mojom::CorsOriginPattern::New(
          scheme, host, /*port=*/0, mode,
          network::mojom::CorsPortMatchMode::kAllowAnyPort,
          network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority));

      base::RunLoop run_loop;
      browser()->profile()->SetCorsOriginAccessListForOrigin(
          url::Origin::Create(embedded_test_server()->base_url().GetOrigin()),
          std::move(list), std::vector<network::mojom::CorsOriginPatternPtr>(),
          run_loop.QuitClosure());
      run_loop.Run();
    }

    {
      std::vector<network::mojom::CorsOriginPatternPtr> list;
      list.push_back(network::mojom::CorsOriginPattern::New(
          scheme, host, /*port=*/0, mode,
          network::mojom::CorsPortMatchMode::kAllowAnyPort,
          network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority));

      base::RunLoop run_loop;
      browser()->profile()->SetCorsOriginAccessListForOrigin(
          url::Origin::Create(
              embedded_test_server()->GetURL(kTestHost, "/").GetOrigin()),
          std::move(list), std::vector<network::mojom::CorsOriginPatternPtr>(),
          run_loop.QuitClosure());
      run_loop.Run();
    }
  }

  std::string host_ip() { return embedded_test_server()->base_url().host(); }

  const base::string16& pass_string() const { return pass_string_; }
  const base::string16& fail_string() const { return fail_string_; }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());

    // Setup to resolve kTestHost, kTestHostInDifferentCase and
    // kTestSubdomainHost to the 127.0.0.1 that the test server serves.
    host_resolver()->AddRule(kTestHost,
                             embedded_test_server()->host_port_pair().host());
    host_resolver()->AddRule(kTestHostInDifferentCase,
                             embedded_test_server()->host_port_pair().host());
    host_resolver()->AddRule(kTestSubdomainHost,
                             embedded_test_server()->host_port_pair().host());
  }

  const base::string16 pass_string_ = base::ASCIIToUTF16("PASS");
  const base::string16 fail_string_ = base::ASCIIToUTF16("FAIL");
  const base::string16 script_ = base::ASCIIToUTF16("reason");

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(CorsOriginAccessListBrowserTest);
};

// Tests if specifying only protocol allows all hosts to pass.
IN_PROC_BROWSER_TEST_F(CorsOriginAccessListBrowserTest, AllowAll) {
  SetAllowList("http", "", kAllowSubdomains);

  std::unique_ptr<content::TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(web_contents(),
                            embedded_test_server()->GetURL(base::StringPrintf(
                                "%s?target=%s", kTestPath, kTestHost))));
  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if specifying only protocol allows all IP address based hosts to pass.
IN_PROC_BROWSER_TEST_F(CorsOriginAccessListBrowserTest, AllowAllForIp) {
  SetAllowList("http", "", kAllowSubdomains);

  std::unique_ptr<content::TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL(
          kTestHost,
          base::StringPrintf("%s?target=%s", kTestPath, host_ip().c_str()))));
  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if complete allow list set allows only exactly matched host to pass.
IN_PROC_BROWSER_TEST_F(CorsOriginAccessListBrowserTest, AllowExactHost) {
  SetAllowList("http", kTestHost, kDisallowSubdomains);

  std::unique_ptr<content::TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(web_contents(),
                            embedded_test_server()->GetURL(base::StringPrintf(
                                "%s?target=%s", kTestPath, kTestHost))));
  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if complete allow list set allows host that matches exactly, but in
// case insensitive way to pass.
IN_PROC_BROWSER_TEST_F(CorsOriginAccessListBrowserTest,
                       AllowExactHostInCaseInsensitive) {
  SetAllowList("http", kTestHost, kDisallowSubdomains);

  std::unique_ptr<content::TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(
      NavigateToURL(web_contents(),
                    embedded_test_server()->GetURL(base::StringPrintf(
                        "%s?target=%s", kTestPath, kTestHostInDifferentCase))));
  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if complete allow list set does not allow a host with a different port
// to pass.
IN_PROC_BROWSER_TEST_F(CorsOriginAccessListBrowserTest, BlockDifferentPort) {
  SetAllowList("http", kTestHost, kDisallowSubdomains);

  std::unique_ptr<content::TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(base::StringPrintf(
                          "%s?target=%s&port_diff=1", kTestPath, kTestHost))));
  EXPECT_EQ(fail_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if complete allow list set allows a subdomain to pass if it is allowed.
IN_PROC_BROWSER_TEST_F(CorsOriginAccessListBrowserTest, AllowSubdomain) {
  SetAllowList("http", kTestHost, kAllowSubdomains);

  std::unique_ptr<content::TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(base::StringPrintf(
                          "%s?target=%s", kTestPath, kTestSubdomainHost))));
  EXPECT_EQ(pass_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if complete allow list set does not allow a subdomain to pass.
IN_PROC_BROWSER_TEST_F(CorsOriginAccessListBrowserTest, BlockSubdomain) {
  SetAllowList("http", kTestHost, kDisallowSubdomains);

  std::unique_ptr<content::TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(base::StringPrintf(
                          "%s?target=%s", kTestPath, kTestSubdomainHost))));
  EXPECT_EQ(fail_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if complete allow list set does not allow a host with a different
// protocol to pass.
IN_PROC_BROWSER_TEST_F(CorsOriginAccessListBrowserTest,
                       BlockDifferentProtocol) {
  SetAllowList("https", kTestHost, kDisallowSubdomains);

  std::unique_ptr<content::TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(web_contents(),
                            embedded_test_server()->GetURL(base::StringPrintf(
                                "%s?target=%s", kTestPath, kTestHost))));
  EXPECT_EQ(fail_string(), watcher->WaitAndGetTitle()) << GetReason();
}

// Tests if IP address based hosts should not follow subdomain match rules.
IN_PROC_BROWSER_TEST_F(CorsOriginAccessListBrowserTest,
                       SubdomainMatchShouldNotBeAppliedForIPAddress) {
  SetAllowList("http", "*.0.0.1", kAllowSubdomains);

  std::unique_ptr<content::TitleWatcher> watcher = CreateWatcher();
  EXPECT_TRUE(NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL(
          kTestHost,
          base::StringPrintf("%s?target=%s", kTestPath, host_ip().c_str()))));
  EXPECT_EQ(fail_string(), watcher->WaitAndGetTitle()) << GetReason();
}

}  // namespace
