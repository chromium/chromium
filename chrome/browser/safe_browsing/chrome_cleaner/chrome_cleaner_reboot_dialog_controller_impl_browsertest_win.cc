// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_reboot_dialog_controller_impl_win.h"

#include <memory>

#include "base/run_loop.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_navigation_util_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/mock_chrome_cleaner_controller_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::StrictMock;
using ::testing::Return;

class MockPromptDelegate
    : public ChromeCleanerRebootDialogControllerImpl::PromptDelegate {
 public:
  MOCK_METHOD2(ShowChromeCleanerRebootPrompt,
               void(Browser* browser,
                    ChromeCleanerRebootDialogControllerImpl* controller));
  MOCK_METHOD0(OnSettingsPageIsActiveTab, void());
};

// The reboot flow requires loading chrome://settings/cleanup, which only
// exists on the Google-branded browser.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

class ChromeCleanerRebootFlowTest : public InProcessBrowserTest {
 public:
  ChromeCleanerRebootFlowTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    // The implementation of dialog_controller_ may check state, and we are not
    // interested in ensuring how many times this is done, since it's not part
    // of the main functionality.
    EXPECT_CALL(mock_cleaner_controller_, state())
        .WillRepeatedly(
            Return(ChromeCleanerController::State::kRebootRequired));
    mock_prompt_delegate_ = std::make_unique<StrictMock<MockPromptDelegate>>();
  }

  void SetUpOnMainThread() override {
    cleanup_settings_page_url_ =
        chrome::GetSettingsUrl(chrome::kCleanupSubPage);

    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void OpenPage(const GURL& gurl, Browser* browser) {
    content::WebContents* contents = chrome::AddSelectedTabWithURL(
        browser, gurl, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
    content::TestNavigationObserver observer(contents);
    observer.Wait();
  }

  Browser* CreateBrowserShowingUrl(const GURL& gurl) {
    Browser* browser = new Browser(
        Browser::CreateParams(ProfileManager::GetActiveUserProfile(), true));
    OpenPage(gurl, browser);
    browser->window()->Show();
    base::RunLoop().RunUntilIdle();
    return browser;
  }

  void SetExpectationsWhenSettingsPageIsActive() {
    EXPECT_CALL(*mock_prompt_delegate_, OnSettingsPageIsActiveTab())
        .WillOnce(InvokeWithoutArgs(
            this,
            &ChromeCleanerRebootFlowTest::RecordRebootPromptStartedAndUnblock));
  }

  void SetExpectationsWhenSettingsPageIsNotActive() {
    EXPECT_CALL(*mock_prompt_delegate_, ShowChromeCleanerRebootPrompt(_, _))
        .WillOnce(InvokeWithoutArgs(
            this,
            &ChromeCleanerRebootFlowTest::RecordRebootPromptStartedAndUnblock));
    // If the prompt dialog is shown, the controller object will only be
    // destroyed after user interaction. This will force the object to be
    // deleted when the test ends.
    close_required_ = true;
  }

  void RecordRebootPromptStartedAndUnblock() {
    reboot_prompt_started_ = true;
    run_loop_->Quit();
  }

  void EnsureCompletedExecution() {
    run_loop_->Run();
    EXPECT_TRUE(reboot_prompt_started_);

    // Force interaction with the prompt to force deleting |dialog_controller_|.
    if (close_required_)
      dialog_controller_->Close();
  }

  GURL cleanup_settings_page_url_;

  StrictMock<MockChromeCleanerController> mock_cleaner_controller_;
  std::unique_ptr<MockPromptDelegate> mock_prompt_delegate_;

  ChromeCleanerRebootDialogControllerImpl* dialog_controller_ = nullptr;
  bool close_required_ = false;
  bool reboot_prompt_started_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
};

IN_PROC_BROWSER_TEST_F(ChromeCleanerRebootFlowTest,
                       OnRebootRequired_SettingsPageActive) {
  SetExpectationsWhenSettingsPageIsActive();

  OpenPage(cleanup_settings_page_url_, browser());
  ASSERT_TRUE(chrome_cleaner_util::CleanupPageIsActiveTab(browser()));

  ChromeCleanerRebootDialogControllerImpl::Create(
      &mock_cleaner_controller_, std::move(mock_prompt_delegate_));

  EnsureCompletedExecution();
}

IN_PROC_BROWSER_TEST_F(ChromeCleanerRebootFlowTest,
                       OnRebootRequired_SettingsPageActiveWhenBrowserIsOpened) {
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::BROWSER, KeepAliveRestartOption::DISABLED);

  SetExpectationsWhenSettingsPageIsActive();

  CloseAllBrowsers();
  base::RunLoop().RunUntilIdle();

  ChromeCleanerRebootDialogControllerImpl::Create(
      &mock_cleaner_controller_, std::move(mock_prompt_delegate_));

  EXPECT_FALSE(reboot_prompt_started_);
  Browser* browser = CreateBrowserShowingUrl(cleanup_settings_page_url_);
  ASSERT_TRUE(chrome_cleaner_util::CleanupPageIsActiveTab(browser));

  EnsureCompletedExecution();
}

IN_PROC_BROWSER_TEST_F(ChromeCleanerRebootFlowTest,
                       OnRebootRequired_SettingsPageNotActive) {
  SetExpectationsWhenSettingsPageIsNotActive();

  dialog_controller_ = ChromeCleanerRebootDialogControllerImpl::Create(
      &mock_cleaner_controller_, std::move(mock_prompt_delegate_));

  EnsureCompletedExecution();
}

IN_PROC_BROWSER_TEST_F(
    ChromeCleanerRebootFlowTest,
    OnRebootRequired_SettingsPageNotActiveWhenBrowserIsOpened) {
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::BROWSER, KeepAliveRestartOption::DISABLED);

  SetExpectationsWhenSettingsPageIsNotActive();

  CloseAllBrowsers();
  base::RunLoop().RunUntilIdle();

  dialog_controller_ = ChromeCleanerRebootDialogControllerImpl::Create(
      &mock_cleaner_controller_, std::move(mock_prompt_delegate_));

  EXPECT_FALSE(reboot_prompt_started_);
  CreateBrowserShowingUrl(GURL(url::kAboutBlankURL));

  EnsureCompletedExecution();
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

class ChromeCleanerRebootDialogResponseTest : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    // The implementation of dialog_controller may check state, and we are not
    // interested in ensuring how many times this is done, since it's not part
    // of the main functionality.
    EXPECT_CALL(mock_cleaner_controller_, state())
        .WillRepeatedly(
            Return(ChromeCleanerController::State::kRebootRequired));
  }

  ChromeCleanerRebootDialogControllerImpl* dialog_controller() {
    auto mock_prompt_delegate =
        std::make_unique<StrictMock<MockPromptDelegate>>();
    EXPECT_CALL(*mock_prompt_delegate, ShowChromeCleanerRebootPrompt(_, _))
        .Times(1);
    return ChromeCleanerRebootDialogControllerImpl::Create(
        &mock_cleaner_controller_, std::move(mock_prompt_delegate));
  }

 protected:
  StrictMock<MockChromeCleanerController> mock_cleaner_controller_;
};

IN_PROC_BROWSER_TEST_F(ChromeCleanerRebootDialogResponseTest, Accept) {
  EXPECT_CALL(mock_cleaner_controller_, Reboot());

  dialog_controller()->Accept();
}

IN_PROC_BROWSER_TEST_F(ChromeCleanerRebootDialogResponseTest, Cancel) {
  dialog_controller()->Cancel();
}

IN_PROC_BROWSER_TEST_F(ChromeCleanerRebootDialogResponseTest, Close) {
  dialog_controller()->Close();
}

}  // namespace
}  // namespace safe_browsing
