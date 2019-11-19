// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace content {
class WebContents;
}  // namespace content

namespace payments {
namespace {

class EmptyParametersTest : public PlatformBrowserTest {
 public:
  EmptyParametersTest() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~EmptyParametersTest() override {}

  void SetUpOnMainThread() override {
    https_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments");
    ASSERT_TRUE(https_server_.Start());

    ASSERT_TRUE(content::NavigateToURL(
        GetActiveWebContents(),
        https_server_.GetURL("/empty_parameters_test.html")));

    PlatformBrowserTest::SetUpOnMainThread();
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 private:
  net::EmbeddedTestServer https_server_;

  DISALLOW_COPY_AND_ASSIGN(EmptyParametersTest);
};

IN_PROC_BROWSER_TEST_F(EmptyParametersTest, NoCrash) {
  EXPECT_EQ(true, content::EvalJs(GetActiveWebContents(), "runTest()"));
}

}  // namespace
}  // namespace payments
