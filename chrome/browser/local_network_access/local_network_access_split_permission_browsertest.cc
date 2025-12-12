// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_network_access/local_network_access_browsertest_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"

// Local Network Access browser tests pertaining to splitting the initial
// local-network-access permission into 2 permissions: local-network and
// loopback-network

namespace local_network_access {

namespace {
constexpr char kTreatAsPublicAddressPath[] =
    "/local_network_access/no-favicon-treat-as-public-address.html";

std::string QueryPermissionScript(std::string_view permission) {
  return content::JsReplace(
      "navigator.permissions.query({ name: $1 }).then((result) => "
      "result.state)",
      permission);
}

}  // namespace
class LocalNetworkAccessSplitPermissionOffBrowserTest
    : public LocalNetworkAccessBrowserTestBase {};

class LocalNetworkAccessSplitPermissionOnBrowserTest
    : public LocalNetworkAccessBrowserTestBase {
 private:
  base::test::ScopedFeatureList feature_list_{
      network::features::kLocalNetworkAccessChecksSplitPermissions};
};

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessSplitPermissionOffBrowserTest,
                       QueryPermissions) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL("a.com", kTreatAsPublicAddressPath)));

  ASSERT_EQ("prompt",
            content::EvalJs(web_contents(),
                            QueryPermissionScript("local-network-access")));
  EXPECT_THAT(
      content::EvalJs(web_contents(), QueryPermissionScript("local-network")),
      content::EvalJsResult::IsError());
  EXPECT_THAT(content::EvalJs(web_contents(),
                              QueryPermissionScript("loopback-network")),
              content::EvalJsResult::IsError());
}

IN_PROC_BROWSER_TEST_F(LocalNetworkAccessSplitPermissionOnBrowserTest,
                       QueryPermissions) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      https_server().GetURL("a.com", kTreatAsPublicAddressPath)));

  ASSERT_EQ("prompt",
            content::EvalJs(web_contents(),
                            QueryPermissionScript("local-network-access")));
  ASSERT_EQ("prompt", content::EvalJs(web_contents(),
                                      QueryPermissionScript("local-network")));
  ASSERT_EQ("prompt", content::EvalJs(web_contents(), QueryPermissionScript(
                                                          "loopback-network")));
}

}  // namespace local_network_access
