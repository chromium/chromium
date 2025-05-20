// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/mock_callback.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_desktop_view_manager.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_notice_service.h"
#include "chrome/browser/privacy_sandbox/notice/notice_service_factory.h"
#include "chrome/browser/privacy_sandbox/notice/notice_service_interface.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/platform_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace privacy_sandbox {
namespace {

using ::testing::Mock;

class PrivacySandboxNoticeEntryPointHandlersTest : public InProcessBrowserTest {
 public:
  PrivacySandboxNoticeEntryPointHandlersTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    https_test_server()->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    ASSERT_TRUE(https_test_server()->Start());

    mock_notice_service_ = static_cast<MockPrivacySandboxNoticeService*>(
        PrivacySandboxNoticeServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                browser()->profile(),
                base::BindRepeating(&BuildMockPrivacySandboxNoticeService)));

    mock_view_manager_ = std::make_unique<MockDesktopViewManager>();

    ON_CALL(*mock_notice_service_, GetDesktopViewManager())
        .WillByDefault(testing::Return(mock_view_manager_.get()));
  }

  void TearDownOnMainThread() override {
    mock_notice_service_ = nullptr;
    mock_view_manager_.reset();
  }

  net::EmbeddedTestServer* https_test_server() { return &https_test_server_; }

 protected:
  raw_ptr<MockPrivacySandboxNoticeService> mock_notice_service_;
  net::EmbeddedTestServer https_test_server_;
  std::unique_ptr<MockDesktopViewManager> mock_view_manager_;
};

// Test that navigation alerts view manager.
IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeEntryPointHandlersTest,
                       TestNavigationCallsEntryPointCallback) {
  EXPECT_CALL(*mock_view_manager_.get(), HandleChromeOwnedPageNavigation)
      .Times(1);
  // Navigate
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  Mock::VerifyAndClearExpectations(mock_view_manager_.get());
}

// Test that navigation alerts view manager.
IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeEntryPointHandlersTest,
                       UnsuitableUrl) {
  EXPECT_CALL(*mock_view_manager_.get(), HandleChromeOwnedPageNavigation)
      .Times(0);

  std::vector<GURL> urls_to_open = {
      https_test_server()->GetURL("a.test", "/title1.html"),
      GURL(chrome::kChromeUISettingsURL).Resolve(chrome::kAutofillSubPage)};

  for (size_t i = 0; i < urls_to_open.size(); ++i) {
    if (i == 0) {
      // Open the first URL in a new tab to trigger a new tab helper.
      ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
          browser(), urls_to_open[i], WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    } else {
      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), urls_to_open[i]));
    }
  }

  Mock::VerifyAndClearExpectations(mock_view_manager_.get());
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeEntryPointHandlersTest,
                       NoPromptInNonNormalBrowser) {
  EXPECT_CALL(*mock_view_manager_.get(), HandleChromeOwnedPageNavigation)
      .Times(0);

  NavigateParams params(browser(), GURL(chrome::kChromeUINewTabPageURL),
                        ui::PAGE_TRANSITION_FIRST);
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.disposition = WindowOpenDisposition::NEW_POPUP;

  ui_test_utils::NavigateToURL(&params);

  Mock::VerifyAndClearExpectations(mock_view_manager_.get());
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeEntryPointHandlersTest,
                       NoPromptInSmallBrowser) {
  EXPECT_CALL(*mock_view_manager_.get(), HandleChromeOwnedPageNavigation)
      .Times(0);

  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 50, 50));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUISettingsURL)));

  Mock::VerifyAndClearExpectations(mock_view_manager_.get());
}

}  // namespace
}  // namespace privacy_sandbox
