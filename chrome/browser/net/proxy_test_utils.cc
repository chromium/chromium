// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/proxy_test_utils.h"

#include "base/test/bind_test_util.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/url_loader_interceptor.h"
#include "google_apis/gaia/gaia_urls.h"

ProxyBrowserTest::ProxyBrowserTest()
    : proxy_server_(net::SpawnedTestServer::TYPE_BASIC_AUTH_PROXY,
                    base::FilePath()) {}

ProxyBrowserTest::~ProxyBrowserTest() {}

void ProxyBrowserTest::SetUp() {
  ASSERT_TRUE(proxy_server_.Start());
  // Block the GaiaAuthFetcher related requests, they somehow interfere with
  // the test when the network service is running.
  url_loader_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) -> bool {
            if (params->url_request.url.host() ==
                GaiaUrls::GetInstance()->gaia_url().host()) {
              return true;
            }
            return false;
          }));
  InProcessBrowserTest::SetUp();
}

void ProxyBrowserTest::PostRunTestOnMainThread() {
  url_loader_interceptor_.reset();
  InProcessBrowserTest::PostRunTestOnMainThread();
}

void ProxyBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(switches::kProxyServer,
                                  proxy_server_.host_port_pair().ToString());

  // TODO(https://crbug.com/901896): Don't rely on proxying localhost (Relied
  // on by BasicAuthWSConnect)
  command_line->AppendSwitchASCII(
      switches::kProxyBypassList,
      net::ProxyBypassRules::GetRulesToSubtractImplicit());
}
