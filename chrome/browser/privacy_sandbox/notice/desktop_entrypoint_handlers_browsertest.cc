// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/desktop_entrypoint_handlers.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_desktop_view_manager.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_notice_service.h"
#include "chrome/browser/privacy_sandbox/notice/notice_service_factory.h"
#include "chrome/browser/privacy_sandbox/notice/notice_service_interface.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
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
  }

  void TearDownOnMainThread() override {
    mock_notice_service_ = nullptr;
  }

  net::EmbeddedTestServer* https_test_server() { return &https_test_server_; }

  MockDesktopViewManager* mock_view_manager() {
    return static_cast<MockDesktopViewManager*>(
        mock_notice_service_->GetDesktopViewManager());
  }

 protected:
  raw_ptr<MockPrivacySandboxNoticeService> mock_notice_service_;
  net::EmbeddedTestServer https_test_server_;
};

// Test that navigation to unsuitable URLS do not alert view manager.
IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeEntryPointHandlersTest,
                       UnsuitableUrl) {
  EXPECT_CALL(*mock_view_manager(), HandleChromeOwnedPageNavigation).Times(0);

  std::vector<GURL> urls_to_open = {
      https_test_server()->GetURL("a.test", "/title1.html"),
      GURL(chrome::kChromeUISettingsURL).Resolve(chrome::kAutofillSubPage)};

  for (size_t i = 0; i < urls_to_open.size(); ++i) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), urls_to_open[i]));
  }

  Mock::VerifyAndClearExpectations(mock_view_manager());
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeEntryPointHandlersTest,
                       NoPromptInNonNormalBrowser) {
  EXPECT_CALL(*mock_view_manager(), HandleChromeOwnedPageNavigation).Times(0);

  NavigateParams params(browser(), GURL(chrome::kChromeUINewTabPageURL),
                        ui::PAGE_TRANSITION_FIRST);
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.disposition = WindowOpenDisposition::NEW_POPUP;

  ui_test_utils::NavigateToURL(&params);

  Mock::VerifyAndClearExpectations(mock_view_manager());
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeEntryPointHandlersTest,
                       NoPromptInSmallBrowser) {
  EXPECT_CALL(*mock_view_manager(), HandleChromeOwnedPageNavigation).Times(0);

  ui_test_utils::SetAndWaitForBounds(*browser(), gfx::Rect(0, 0, 50, 50));

// Resizing does not work on Mac because of minimum window height. Ensure the
// minimum height is still > 100, then skip test.
#if BUILDFLAG(IS_MAC)
  ASSERT_TRUE(browser()->window()->GetBounds().height() > 100);
  GTEST_SKIP();
#endif

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUISettingsURL)));

  Mock::VerifyAndClearExpectations(mock_view_manager());
}

class PrivacySandboxNoticeEntryPointHandlersTest_SuitableUrls
    : public PrivacySandboxNoticeEntryPointHandlersTest,
      public testing::WithParamInterface<GURL> {};

// Test that navigation to suitable URLS alert view manager.
IN_PROC_BROWSER_TEST_P(PrivacySandboxNoticeEntryPointHandlersTest_SuitableUrls,
                       SuitableUrl) {
  GURL url_to_open = GetParam();

  EXPECT_CALL(*mock_view_manager(), HandleChromeOwnedPageNavigation).Times(1);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_to_open));

  Mock::VerifyAndClearExpectations(mock_view_manager());
}

// Define the test parameters.
INSTANTIATE_TEST_SUITE_P(
    AllSuitableUrls,
    PrivacySandboxNoticeEntryPointHandlersTest_SuitableUrls,
    testing::Values(GURL(chrome::kChromeUINewTabURL),
                    GURL(chrome::kChromeUINewTabPageURL),
                    GURL(url::kAboutBlankURL),
                    GURL(chrome::kChromeUISettingsURL),
                    GURL(chrome::kChromeUIHistoryURL)));

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Check when profile setup is in progress, that no prompt is shown.
IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeEntryPointHandlersTest,
                       NoPromptProfileSetup) {
  EXPECT_CALL(*mock_view_manager(), HandleChromeOwnedPageNavigation).Times(0);
  // Show the profile customization dialog.
  browser()->signin_view_controller()->ShowModalProfileCustomizationDialog(
      /*is_local_profile_creation=*/true);
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  Mock::VerifyAndClearExpectations(mock_view_manager());
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace
}  // namespace privacy_sandbox
