// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PROXY_TEST_UTILS_H_
#define CHROME_BROWSER_NET_PROXY_TEST_UTILS_H_

#include "chrome/test/base/in_process_browser_test.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

namespace content {
class URLLoaderInterceptor;
}

class ProxyBrowserTest : public InProcessBrowserTest {
 public:
  ProxyBrowserTest();

  ProxyBrowserTest(const ProxyBrowserTest&) = delete;
  ProxyBrowserTest& operator=(const ProxyBrowserTest&) = delete;

  ~ProxyBrowserTest() override;

  // InProcessBrowserTest::
  void SetUp() override;
  void PostRunTestOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

 protected:
  net::SpawnedTestServer proxy_server_;

 private:
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

#endif  // CHROME_BROWSER_NET_PROXY_TEST_UTILS_H_
