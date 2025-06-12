// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/desktop_entrypoint_handlers.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_desktop_view_manager.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_notice_service.h"
#include "chrome/browser/privacy_sandbox/notice/notice_service_factory.h"
#include "chrome/browser/privacy_sandbox/notice/notice_service_interface.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/platform_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace privacy_sandbox {
namespace {

using ::testing::Mock;

class PrivacySandboxNoticeEntryPointHandlersTest : public InProcessBrowserTest {
 public:
  PrivacySandboxNoticeEntryPointHandlersTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{privacy_sandbox::kPrivacySandboxNoticeFramework,
                               {}}},
        {});
  }

  void RegisterTestingSyncServiceFactory(content::BrowserContext* context) {
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::TestSyncService>();
            }));
  }

  void SetUpInProcessBrowserTestFixture() override {
    services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &PrivacySandboxNoticeEntryPointHandlersTest::
                    RegisterTestingSyncServiceFactory,
                base::Unretained(this)));
  }

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

  syncer::TestSyncService* test_sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(browser()->profile()));
  }

 protected:
  raw_ptr<MockPrivacySandboxNoticeService> mock_notice_service_;
  net::EmbeddedTestServer https_test_server_;
  base::CallbackListSubscription services_subscription_;
  base::test::ScopedFeatureList feature_list_;
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

// The test checks that a prompt is shown on kChromeUINewTabURL navigation.
// For non-ChromeOS platforms this works because kChromeUINewTabURL redirects to
// kChromeUINewTabPageURL according to
// https://g3doc.corp.google.com/chrome/newtab/g3doc/ntp-types.md?cl=head.
// For ChromeOS platforms this works because about:Blank is opened on
// kChromeUINewTabURL navigation, allowing the prompt to show.
IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeEntryPointHandlersTest,
                       PromptShowsNewTabChromeOS) {
  EXPECT_CALL(*mock_view_manager(), HandleChromeOwnedPageNavigation).Times(1);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));

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

IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeEntryPointHandlersTest,
                       NoPromptSync) {
  // Check when sync setup is in progress, that no prompt is shown.
  EXPECT_CALL(*mock_view_manager(), HandleChromeOwnedPageNavigation).Times(0);

  test_sync_service()->SetSetupInProgress();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUISettingsURL)));

  Mock::VerifyAndClearExpectations(mock_view_manager());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Check when profile setup is in progress, that no prompt is shown.
IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeEntryPointHandlersTest,
                       NoPromptProfileSetup) {
  EXPECT_CALL(*mock_view_manager(), HandleChromeOwnedPageNavigation).Times(0);
  // Show the profile customization dialog.
  browser()
      ->GetFeatures()
      .signin_view_controller()
      ->ShowModalProfileCustomizationDialog(
          /*is_local_profile_creation=*/true);
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  Mock::VerifyAndClearExpectations(mock_view_manager());
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// SearchEngineChoiceCheck
class PrivacySandboxNoticeEntryPointHandlersTest_SearchEngineChoiceDialog
    : public PrivacySandboxNoticeEntryPointHandlersTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kSearchEngineChoiceCountry, "BE");
    command_line->AppendSwitch(
        switches::kIgnoreNoFirstRunForSearchEngineChoiceScreen);
  }

  void SetUpOnMainThread() override {
    PrivacySandboxNoticeEntryPointHandlersTest::SetUpOnMainThread();
    SearchEngineChoiceDialogService::SetDialogDisabledForTests(
        /*dialog_disabled=*/false);
  }

 private:
  base::AutoReset<bool> scoped_chrome_build_override_ =
      SearchEngineChoiceDialogServiceFactory::
          ScopedChromeBuildOverrideForTesting(
              /*force_chrome_build=*/true);
};

IN_PROC_BROWSER_TEST_F(
    PrivacySandboxNoticeEntryPointHandlersTest_SearchEngineChoiceDialog,
    NoPromptSearchEngineChoiceDialog) {
  // Check when sync setup is in progress, that no prompt is shown.
  EXPECT_CALL(*mock_view_manager(), HandleChromeOwnedPageNavigation).Times(0);

  // Navigate to a page where the DMA notice should show.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  // Make a search engine choice to close the dialog.
  SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(
          browser()->profile());
  search_engine_choice_dialog_service->NotifyChoiceMade(
      /*prepopulate_id=*/1, /*save_guest_mode_selection=*/false,
      SearchEngineChoiceDialogService::EntryPoint::kDialog);

  // Make sure the Privacy Sandbox prompt doesn't get displayed on the next
  // navigation.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUISettingsURL)));

  Mock::VerifyAndClearExpectations(mock_view_manager());
}

// URLS check
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
}  // namespace
}  // namespace privacy_sandbox
