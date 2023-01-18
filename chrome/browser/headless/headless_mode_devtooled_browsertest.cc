// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_devtooled_browsertest.h"

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/headless/test/headless_browser_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"

namespace headless {

HeadlessDevTooledBrowserTest::HeadlessDevTooledBrowserTest() = default;
HeadlessDevTooledBrowserTest::~HeadlessDevTooledBrowserTest() = default;

void HeadlessDevTooledBrowserTest::RunTest() {
  ASSERT_TRUE(embedded_test_server()->Start());

  browser_devtools_client_.AttachToBrowser();

  content::BrowserContext* browser_context = browser()->profile();
  DCHECK(browser_context);

  content::WebContents::CreateParams create_params(browser_context);
  web_contents_ = content::WebContents::Create(create_params);
  DCHECK(web_contents_);

  content::WebContentsObserver::Observe(web_contents_.get());

  devtools_client_.AttachToWebContents(web_contents_.get());

  GURL url = GetURL();
  content::NavigationController::LoadURLParams params(url);
  web_contents_->GetController().LoadURLWithParams(params);

  RunAsyncTest();

  devtools_client_.DetachClient();

  content::WebContentsObserver::Observe(nullptr);

  web_contents_->Close();
  web_contents_.reset();

  browser_devtools_client_.DetachClient();

  base::RunLoop().RunUntilIdle();
}

void HeadlessDevTooledBrowserTest::RenderViewReady() {
  RunDevTooledTest();
}

void HeadlessDevTooledBrowserTest::WebContentsDestroyed() {
  FinishAsyncTest();
  FAIL() << "Web contents destroyed unexpectedly.";
}

void HeadlessDevTooledBrowserTest::RunAsyncTest() {
  DCHECK(!run_loop_);

  run_loop_ = std::make_unique<base::RunLoop>(
      base::RunLoop::Type::kNestableTasksAllowed);
  run_loop_->Run();
  run_loop_ = nullptr;
}

void HeadlessDevTooledBrowserTest::FinishAsyncTest() {
  DCHECK(run_loop_);

  run_loop_->Quit();
}

GURL HeadlessDevTooledBrowserTest::GetURL() {
  return GURL("about:blank");
}

class HeadlessDevToolsClientNavigationTest
    : public HeadlessDevTooledBrowserTest {
 public:
  void RunDevTooledTest() override {
    devtools_client_.AddEventHandler(
        "Page.loadEventFired",
        base::BindRepeating(
            &HeadlessDevToolsClientNavigationTest::OnLoadEventFired,
            base::Unretained(this)));
    SendCommandSync(devtools_client_, "Page.enable");

    devtools_client_.SendCommand(
        "Page.navigate",
        Param("url", embedded_test_server()->GetURL("/hello.html").spec()));
  }

  void OnLoadEventFired(const base::Value::Dict& params) {
    devtools_client_.SendCommand("Page.disable");

    FinishAsyncTest();
  }
};

HEADLESS_DEVTOOLED_TEST_F(HeadlessDevToolsClientNavigationTest);

}  // namespace headless
