// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"

#include "chrome/common/webui_url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

class ChromeExtensionsAPIClientTest : public testing::Test {
 public:
  ChromeExtensionsAPIClientTest() = default;

  ChromeExtensionsAPIClientTest(const ChromeExtensionsAPIClientTest&) = delete;
  ChromeExtensionsAPIClientTest& operator=(
      const ChromeExtensionsAPIClientTest&) = delete;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ChromeExtensionsAPIClientTest, ShouldHideResponseHeader) {
  ChromeExtensionsAPIClient client;
  EXPECT_TRUE(client.ShouldHideResponseHeader(
      GaiaUrls::GetInstance()->gaia_url(), "X-Chrome-ID-Consistency-Response"));
  EXPECT_TRUE(client.ShouldHideResponseHeader(
      GaiaUrls::GetInstance()->gaia_url(), "x-cHroMe-iD-CoNsiStenCY-RESPoNSE"));
  EXPECT_FALSE(client.ShouldHideResponseHeader(
      GURL("http://www.example.com"), "X-Chrome-ID-Consistency-Response"));
  EXPECT_FALSE(client.ShouldHideResponseHeader(
      GaiaUrls::GetInstance()->gaia_url(), "Google-Accounts-SignOut"));
}

TEST_F(ChromeExtensionsAPIClientTest, ShouldHideBrowserNetworkRequest) {
  ChromeExtensionsAPIClient client;

  auto create_params = [](WebRequestResourceType web_request_type) {
    WebRequestInfoInitParams request_params;
    request_params.url = GURL("https://example.com/script.js");
    request_params.initiator =
        url::Origin::Create(GURL(chrome::kChromeUINewTabURL));
    request_params.render_process_id = -1;
    request_params.web_request_type = web_request_type;
    return request_params;
  };

  // Requests made by the browser with chrome://newtab as its initiator should
  // not be visible to extensions.
  EXPECT_TRUE(client.ShouldHideBrowserNetworkRequest(
      nullptr /* context */,
      WebRequestInfo(create_params(WebRequestResourceType::SCRIPT))));

  // Main frame requests should always be visible to extensions.
  EXPECT_FALSE(client.ShouldHideBrowserNetworkRequest(
      nullptr /* context */,
      WebRequestInfo(create_params(WebRequestResourceType::MAIN_FRAME))));

  // Similar requests made by the renderer should be visible to extensions.
  WebRequestInfoInitParams params =
      create_params(WebRequestResourceType::SCRIPT);
  params.render_process_id = 2;
  EXPECT_FALSE(client.ShouldHideBrowserNetworkRequest(
      nullptr /* context */, WebRequestInfo(std::move(params))));
}

}  // namespace extensions
