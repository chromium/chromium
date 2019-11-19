// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_host.h"

#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::WebContents;

namespace safe_browsing {
namespace {

class TestClientSideDetectionHost : public ClientSideDetectionHost {
 public:
  explicit TestClientSideDetectionHost(WebContents* tab)
      : ClientSideDetectionHost(tab) {}

 private:
  void OnMalwarePreClassificationDone(bool should_classify_not_used) override {
    ClientSideDetectionHost::OnMalwarePreClassificationDone(true);
  }

  void DidStopLoading() override {}
};

bool FindExpectedIPUrlInfo(const IPUrlInfo& expected_info,
                           const std::vector<IPUrlInfo>& ip_url_vector) {
  auto result = std::find_if(
      ip_url_vector.begin(), ip_url_vector.end(),
      [expected_info](const IPUrlInfo& ip_url_info) {
        return expected_info.url == ip_url_info.url &&
               expected_info.method == ip_url_info.method &&
               expected_info.referrer == ip_url_info.referrer &&
               expected_info.resource_type == ip_url_info.resource_type;
      });
  return result != ip_url_vector.end();
}

}  // namespace

using ClientSideDetectionHostBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ClientSideDetectionHostBrowserTest,
                       VerifyIPAddressCollection) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                               false);
  ASSERT_TRUE(embedded_test_server()->Start());
  std::unique_ptr<TestClientSideDetectionHost> csd_host =
      std::make_unique<TestClientSideDetectionHost>(
          browser()->tab_strip_model()->GetActiveWebContents());
  GURL page_url(embedded_test_server()->GetURL("/safe_browsing/malware.html"));
  ui_test_utils::NavigateToURL(browser(), page_url);

  BrowseInfo* browse_info = csd_host->GetBrowseInfo();
  EXPECT_EQ(1u, browse_info->ips.size());
  const std::vector<IPUrlInfo>& ip_urls =
      browse_info->ips[embedded_test_server()->base_url().host()];
  IPUrlInfo expected_result_1(
      embedded_test_server()->GetURL("/safe_browsing/malware_image.png").spec(),
      "GET", page_url.spec(), content::ResourceType::kImage);
  IPUrlInfo expected_result_2(embedded_test_server()
                                  ->GetURL("/safe_browsing/malware_iframe.html")
                                  .spec(),
                              "GET", page_url.spec(),
                              content::ResourceType::kSubFrame);
  EXPECT_TRUE(FindExpectedIPUrlInfo(expected_result_1, ip_urls));
  EXPECT_TRUE(FindExpectedIPUrlInfo(expected_result_2, ip_urls));
}

}  // namespace safe_browsing
