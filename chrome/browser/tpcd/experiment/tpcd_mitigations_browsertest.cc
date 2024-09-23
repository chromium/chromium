// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"

namespace {

constexpr char kHostA[] = "a.test";
constexpr char kHostB[] = "b.test";

using testing::Bool;
using testing::HasSubstr;

class NativeUnpartitionedStorageAccessWhen3PCOff
    : public policy::PolicyTest,
      public testing::WithParamInterface<
          /*content_settings::features::
                       kNativeUnpartitionedStoragePermittedWhen3PCOff*/
          bool> {
 protected:
  NativeUnpartitionedStorageAccessWhen3PCOff() = default;

  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    if (NativeUnpartitionedStoragePermittedWhenTpcOff()) {
      enabled_features = {
          content_settings::features::kTrackingProtection3pcd,
          content_settings::features::
              kNativeUnpartitionedStoragePermittedWhen3PCOff,
      };
    } else {
      enabled_features = {content_settings::features::kTrackingProtection3pcd};
    }
    features_.InitWithFeatures(enabled_features,
                               {net::features::kThirdPartyStoragePartitioning});
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromDirectory(path);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
  }

  void NavigateToPage(const std::string& host, const std::string& path) {
    GURL main_url(https_server_.GetURL(host, path));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  }

  void NavigateFrameTo(const std::string& host, const std::string& path) {
    GURL url = https_server_.GetURL(host, path);
    content::WebContents* web_contents =
        (browser())->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(NavigateIframeToURL(web_contents, /*iframe_id*/ "test", url));
  }

  content::RenderFrameHost* GetPrimaryMainFrame() {
    content::WebContents* web_contents =
        (browser())->tab_strip_model()->GetActiveWebContents();
    return web_contents->GetPrimaryMainFrame();
  }

  content::RenderFrameHost* GetChildFrame() {
    return ChildFrameAt(GetPrimaryMainFrame(), 0);
  }

  bool NativeUnpartitionedStoragePermittedWhenTpcOff() const {
    return GetParam();
  }

 private:
  net::test_server::EmbeddedTestServer https_server_{
      net::EmbeddedTestServer::TYPE_HTTPS};
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(NativeUnpartitionedStorageTest,
                         NativeUnpartitionedStorageAccessWhen3PCOff,
                         Bool());

IN_PROC_BROWSER_TEST_P(NativeUnpartitionedStorageAccessWhen3PCOff,
                       CrossOriginsAccessWithCookieBlocking) {
  // Navigate to ORIGIN B and add a value to local storage
  NavigateToPage(kHostB, "/browsing_data/site_data.html");
  EXPECT_TRUE(ExecJs(GetPrimaryMainFrame(),
                     "localStorage.setItem('UnpartitionedLocal', 'Hello');"));

  // Navigate to ORIGIN A with a child frame pointing to ORIGIN B
  NavigateToPage(kHostA, "/iframe.html");
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");

  // check unpartitioned storage access when the
  // kNativeUnpartitionedStoragePermittedWhen3PCOff feature is enabled.
  if (NativeUnpartitionedStoragePermittedWhenTpcOff()) {
    EXPECT_EQ(content::EvalJs(GetChildFrame(),
                              "localStorage.getItem(\"UnpartitionedLocal\")"),
              "Hello");
  } else {
    EXPECT_THAT(
        content::EvalJs(GetChildFrame(),
                        "localStorage.getItem(\"UnpartitionedLocal\")")
            .error,
        HasSubstr("Error: Failed to read the 'localStorage' property from "
                  "'Window': Access is denied for this document."));
  }
}

}  // namespace
