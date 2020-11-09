// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/test/bind_test_util.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/net/safe_search_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/google/core/common/google_switches.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "url/gurl.h"

namespace policy {

class PolicyTestGoogle : public PolicyTest {
 public:
  PolicyTestGoogle() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  std::map<std::string, net::HttpRequestHeaders> urls_requested() {
    base::AutoLock auto_lock(lock_);
    return urls_requested_;
  }

 private:
  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();

    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    https_server_.RegisterRequestMonitor(base::BindLambdaForTesting(
        [&](const net::test_server::HttpRequest& request) {
          net::HttpRequestHeaders headers;
          for (auto& header : request.headers)
            headers.SetHeader(header.first, header.second);
          base::AutoLock auto_lock(lock_);
          urls_requested_[request.relative_url] = headers;
        }));

    ASSERT_TRUE(https_server_.Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Note for the google and youtube tests below, the throttles expect that
    // the URLs are to google.com or youtube.com. Networking code also
    // automatically upgrades http requests to these domains to https (see the
    // preload list in https://www.chromium.org/hsts). So as a result we need
    // to make the requests to an https server. Since the HTTPS server only
    // serves a valid cert for localhost, so this is needed to load pages from
    // "www.google.com" without an interstitial.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);

    // The production code only allows known ports (80 for http and 443 for
    // https), but the test server runs on a random port.
    command_line->AppendSwitch(switches::kIgnoreGooglePortNumbers);
  }

  net::EmbeddedTestServer https_server_;
  base::Lock lock_;
  std::map<std::string, net::HttpRequestHeaders> urls_requested_;
};

IN_PROC_BROWSER_TEST_F(PolicyTestGoogle, ForceGoogleSafeSearch) {
  ApplySafeSearchPolicy(base::nullopt,  // ForceSafeSearch
                        base::Value(true),
                        base::nullopt,   // ForceYouTubeSafetyMode
                        base::nullopt);  // ForceYouTubeRestrict

  GURL url = https_server()->GetURL("www.google.com",
                                    "/server-redirect?http://google.com/");
  CheckSafeSearch(browser(), true, url.spec());
}

IN_PROC_BROWSER_TEST_F(PolicyTestGoogle, ForceYouTubeRestrict) {
  for (int youtube_restrict_mode = safe_search_util::YOUTUBE_RESTRICT_OFF;
       youtube_restrict_mode < safe_search_util::YOUTUBE_RESTRICT_COUNT;
       ++youtube_restrict_mode) {
    ApplySafeSearchPolicy(base::nullopt,  // ForceSafeSearch
                          base::nullopt,  // ForceGoogleSafeSearch
                          base::nullopt,  // ForceYouTubeSafetyMode
                          base::Value(youtube_restrict_mode));
    {
      // First check frame requests.
      GURL youtube_url(https_server()->GetURL("youtube.com", "/empty.html"));
      ui_test_utils::NavigateToURL(browser(), youtube_url);

      CheckYouTubeRestricted(youtube_restrict_mode,
                             urls_requested()[youtube_url.path()]);
    }

    {
      // Now check subresource loads.
      GURL youtube_script(https_server()->GetURL("youtube.com", "/json2.js"));
      FetchSubresource(browser()->tab_strip_model()->GetActiveWebContents(),
                       youtube_script);

      CheckYouTubeRestricted(youtube_restrict_mode,
                             urls_requested()[youtube_script.path()]);
    }
  }
}

IN_PROC_BROWSER_TEST_F(PolicyTestGoogle, AllowedDomainsForApps) {
  for (int allowed_domains = 0; allowed_domains < 2; ++allowed_domains) {
    std::string allowed_domain;
    if (allowed_domains) {
      PolicyMap policies;
      allowed_domain = "foo.com";
      SetPolicy(&policies, key::kAllowedDomainsForApps,
                base::Value(allowed_domain));
      UpdateProviderPolicy(policies);
    }

    {
      // First check frame requests.
      GURL google_url = https_server()->GetURL("google.com", "/empty.html");
      ui_test_utils::NavigateToURL(browser(), google_url);

      CheckAllowedDomainsHeader(allowed_domain,
                                urls_requested()[google_url.path()]);
    }

    {
      // Now check subresource loads.
      GURL google_script =
          https_server()->GetURL("google.com", "/result_queue.js");

      FetchSubresource(browser()->tab_strip_model()->GetActiveWebContents(),
                       google_script);

      CheckAllowedDomainsHeader(allowed_domain,
                                urls_requested()[google_script.path()]);
    }

    {
      // Double check that a frame to a non-Google url doesn't have the header.
      GURL non_google_url = https_server()->GetURL("/empty.html");
      ui_test_utils::NavigateToURL(browser(), non_google_url);

      CheckAllowedDomainsHeader(std::string(),
                                urls_requested()[non_google_url.path()]);
    }
  }
}

}  // namespace policy
