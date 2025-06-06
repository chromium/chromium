// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/desktop_entrypoint_handlers.h"
#include "chrome/browser/privacy_sandbox/notice/desktop_view_manager_test_peer.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_desktop_view_manager.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_notice_service.h"
#include "chrome/browser/privacy_sandbox/notice/notice_service_factory.h"
#include "chrome/browser/privacy_sandbox/notice/notice_service_interface.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_prompt.h"
#include "chrome/browser/ui/views/privacy_sandbox/privacy_sandbox_dialog_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/platform_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {
using privacy_sandbox::notice::mojom::PrivacySandboxNotice;
}

namespace privacy_sandbox {
namespace {

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;

class PrivacySandboxNoticeViewManagerTest : public InProcessBrowserTest {
 public:
  PrivacySandboxNoticeViewManagerTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{privacy_sandbox::kPrivacySandboxNoticeFramework,
                               {}}},
        {});
  }

  void SetUpOnMainThread() override {
    mock_notice_service_ = static_cast<MockPrivacySandboxNoticeService*>(
        PrivacySandboxNoticeServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                browser()->profile(),
                base::BindRepeating(&BuildMockPrivacySandboxNoticeService)));

    desktop_view_manager_ =
        std::make_unique<DesktopViewManager>(mock_notice_service_.get());

    ON_CALL(*mock_notice_service_, GetDesktopViewManager())
        .WillByDefault(Return(desktop_view_manager_.get()));

    desktop_view_manager_test_peer_ =
        std::make_unique<DesktopViewManagerTestPeer>(
            desktop_view_manager_.get());
  }

  void TearDownOnMainThread() override {
    desktop_view_manager_test_peer_.reset();
    desktop_view_manager_.reset();
    mock_notice_service_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();  // Call base class TearDown
  }

  DesktopViewManager* desktop_view_manager() {
    return desktop_view_manager_.get();
  }

  void SetRequiredNotices(std::vector<PrivacySandboxNotice> required_notices) {
    ON_CALL(*mock_notice_service_, GetRequiredNotices(_))
        .WillByDefault(Return(required_notices));
  }

  bool IsPromptShowingOnBrowser(Browser* browser) {
    return desktop_view_manager_test_peer_->IsPromptShowingOnBrowser(browser);
  }

  void HandleChromeOwnedPageNavigation() {
    desktop_view_manager_test_peer_->HandleChromeOwnedPageNavigation(browser());
  }

 protected:
  raw_ptr<MockPrivacySandboxNoticeService> mock_notice_service_;
  std::unique_ptr<DesktopViewManager> desktop_view_manager_;
  std::unique_ptr<DesktopViewManagerTestPeer> desktop_view_manager_test_peer_;
  base::test::ScopedFeatureList feature_list_;
};

// Test that browsers are registered and unregistered correctly.
IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeViewManagerTest,
                       IsShowingPrompt_SingleWindow) {
  SetRequiredNotices({PrivacySandboxNotice::kTopicsConsentNotice});

  views::NamedWidgetShownWaiter waiter1(
      views::test::AnyWidgetTestPasskey{},
      PrivacySandboxDialogView::kViewClassName);
  desktop_view_manager()->HandleChromeOwnedPageNavigation(browser());

  auto* dialog1 = waiter1.WaitIfNeededAndGet();
  auto* view1 = static_cast<PrivacySandboxDialogView*>(
      dialog1->widget_delegate()->GetContentsView());

  ASSERT_TRUE(IsPromptShowingOnBrowser(browser()));

  view1->CloseNativeView();

  // Must manually close the dialog for before test destruction begins.
  dialog1->CloseNow();

  ASSERT_FALSE(IsPromptShowingOnBrowser(browser()));
}

// Test that browsers on multiple windows are registered correctly.
IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeViewManagerTest,
                       IsShowingPrompt_MultiWindow) {
  SetRequiredNotices({PrivacySandboxNotice::kTopicsConsentNotice});

  views::NamedWidgetShownWaiter waiter1(
      views::test::AnyWidgetTestPasskey{},
      PrivacySandboxDialogView::kViewClassName);

  desktop_view_manager()->HandleChromeOwnedPageNavigation(browser());

  auto* dialog1 = waiter1.WaitIfNeededAndGet();
  auto* view1 = static_cast<PrivacySandboxDialogView*>(
      dialog1->widget_delegate()->GetContentsView());

  ASSERT_TRUE(IsPromptShowingOnBrowser(browser()));

  views::NamedWidgetShownWaiter waiter2(
      views::test::AnyWidgetTestPasskey{},
      PrivacySandboxDialogView::kViewClassName);

  auto* new_rfh = ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  auto* new_browser = chrome::FindBrowserWithTab(
      content::WebContents::FromRenderFrameHost(new_rfh));
  PrivacySandboxDialog::Show(new_browser,
                             PrivacySandboxService::PromptType::kM1Consent);

  auto* dialog2 = waiter2.WaitIfNeededAndGet();
  auto* view2 = static_cast<PrivacySandboxDialogView*>(
      dialog2->widget_delegate()->GetContentsView());

  ASSERT_TRUE(IsPromptShowingOnBrowser(new_browser));

  view1->CloseNativeView();
  dialog1->CloseNow();

  ASSERT_FALSE(IsPromptShowingOnBrowser(browser()));

  view2->CloseNativeView();
  dialog2->CloseNow();

  ASSERT_FALSE(IsPromptShowingOnBrowser(new_browser));
}

}  // namespace
}  // namespace privacy_sandbox
