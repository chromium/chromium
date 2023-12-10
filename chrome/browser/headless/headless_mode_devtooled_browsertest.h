// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEADLESS_HEADLESS_MODE_DEVTOOLED_BROWSERTEST_H_
#define CHROME_BROWSER_HEADLESS_HEADLESS_MODE_DEVTOOLED_BROWSERTEST_H_

#include <memory>

#include "base/run_loop.h"
#include "chrome/browser/headless/headless_mode_browsertest.h"
#include "components/devtools/simple_devtools_protocol_client/simple_devtools_protocol_client.h"
#include "content/public/browser/web_contents_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class WebContents;
}

namespace headless {

// Base class for tests that require access to a DevToolsClient. Subclasses
// should override the RunDevTooledTest() method, which is called asynchronously
// when the DevToolsClient is ready.
class HeadlessModeDevTooledBrowserTest : public HeadlessModeBrowserTest,
                                         public content::WebContentsObserver {
 public:
  HeadlessModeDevTooledBrowserTest();
  ~HeadlessModeDevTooledBrowserTest() override;

 protected:
  using SimpleDevToolsProtocolClient =
      simple_devtools_protocol_client::SimpleDevToolsProtocolClient;

  // content::WebContentsObserver implementation:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void WebContentsDestroyed() override;

  // Implemented by tests and used to send requests to DevTools. Subclasses
  // need to ensure that FinishAsyncTest() is called at some point.
  virtual void RunDevTooledTest() = 0;

  // Notify that an asynchronous test is complete and the test runner should
  // exit.
  virtual void FinishAsyncTest();

  // Run an asynchronous test in a nested run loop. The caller should call
  // FinishAsyncTest() to notify that the test should finish.
  void RunAsyncTest();

  void RunTest();

  SimpleDevToolsProtocolClient devtools_client_;
  SimpleDevToolsProtocolClient browser_devtools_client_;
  std::unique_ptr<content::WebContents> web_contents_;

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  bool test_started_ = false;
};

#define HEADLESS_MODE_DEVTOOLED_TEST_F(TEST_FIXTURE_NAME)   \
  IN_PROC_BROWSER_TEST_F(TEST_FIXTURE_NAME, RunAsyncTest) { \
    RunTest();                                              \
  }                                                         \
  class HeadlessModeDevTooledBrowserTestNeedsSemicolon##TEST_FIXTURE_NAME {}

#define HEADLESS_MODE_DEVTOOLED_TEST_P(TEST_FIXTURE_NAME)   \
  IN_PROC_BROWSER_TEST_P(TEST_FIXTURE_NAME, RunAsyncTest) { \
    RunTest();                                              \
  }                                                         \
  class HeadlessModeDevTooledBrowserTestNeedsSemicolon##TEST_FIXTURE_NAME {}

#define DISABLED_HEADLESS_MODE_DEVTOOLED_TEST_F(TEST_FIXTURE_NAME)   \
  IN_PROC_BROWSER_TEST_F(TEST_FIXTURE_NAME, DISABLED_RunAsyncTest) { \
    RunTest();                                                       \
  }                                                                  \
  class HeadlessModeDevTooledBrowserTestNeedsSemicolon##TEST_FIXTURE_NAME {}

#define DISABLED_HEADLESS_MODE_DEVTOOLED_TEST_P(TEST_FIXTURE_NAME)   \
  IN_PROC_BROWSER_TEST_P(TEST_FIXTURE_NAME, DISABLED_RunAsyncTest) { \
    RunTest();                                                       \
  }                                                                  \
  class HeadlessModeDevTooledBrowserTestNeedsSemicolon##TEST_FIXTURE_NAME {}

}  // namespace headless

#endif  // CHROME_BROWSER_HEADLESS_HEADLESS_MODE_DEVTOOLED_BROWSERTEST_H_
