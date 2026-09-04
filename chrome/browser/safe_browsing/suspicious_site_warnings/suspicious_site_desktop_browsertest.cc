// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/suspicious_site_warnings/suspicious_site_controller_desktop.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ssl/chrome_security_state_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/location_bar/location_icon_test_accessor.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/browser/ui/views/page_info/suspicious_site_bubble_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/db/fake_database_manager.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/suspicious_site_warning_allowlist.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_state/core/security_state.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/widget.h"

namespace safe_browsing {

class SuspiciousSiteDesktopBrowserTest : public InProcessBrowserTest {
 public:
  SuspiciousSiteDesktopBrowserTest() {
    feature_list_.InitAndEnableFeature(kSuspiciousSiteWarnings);
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    InProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);
    factory_.SetTestUIManager(new TestSafeBrowsingUIManager());
    factory_.SetTestDatabaseManager(new FakeSafeBrowsingDatabaseManager(
        content::GetUIThreadTaskRunner({})));
    SafeBrowsingService::RegisterFactory(&factory_);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    safe_browsing::SetSafeBrowsingState(
        browser()->GetProfile()->GetPrefs(),
        safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDown() override {
    SuspiciousSiteControllerDesktop::SetBubbleShownCallbackForTesting(
        base::NullCallback());
    SuspiciousSiteControllerDesktop::SetBubbleDestroyedCallbackForTesting(
        base::NullCallback());
    InProcessBrowserTest::TearDown();
    SafeBrowsingService::RegisterFactory(nullptr);
  }

  void SetURLThreatType(const GURL& url, SBThreatType threat_type) {
    TestSafeBrowsingService* service = factory_.test_safe_browsing_service();
    ASSERT_TRUE(service);

    static_cast<FakeSafeBrowsingDatabaseManager*>(
        service->database_manager().get())
        ->AddDangerousUrl(url, threat_type);
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

  void RunCheckWarningWithProtectionLevelTest(
      safe_browsing::SafeBrowsingState state,
      bool expect_warning) {
    safe_browsing::SetSafeBrowsingState(browser()->GetProfile()->GetPrefs(),
                                        state);

    GURL malicious_url = embedded_test_server()->GetURL("/title1.html");
    SetURLThreatType(malicious_url,
                     SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);

    base::test::TestFuture<void> shown_future;
    SuspiciousSiteControllerDesktop::SetBubbleShownCallbackForTesting(
        shown_future.GetCallback());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), malicious_url));

    if (expect_warning) {
      EXPECT_TRUE(shown_future.Wait());
      EXPECT_TRUE(
          safe_browsing::SuspiciousSiteControllerDesktop::FromWebContents(
              GetActiveWebContents()));

      views::BubbleDialogDelegateView* bubble =
          PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
      ASSERT_TRUE(bubble);
      ASSERT_TRUE(bubble->GetWidget());
      EXPECT_TRUE(bubble->GetWidget()->IsVisible());

      EXPECT_TRUE(GetActiveWebContents()->ShouldIgnoreInputEventsForTesting());
      EXPECT_TRUE(browser()->GetTabStripModel()->IsTabBlocked(0));

      auto security_state = chrome_security_state::GetVisibleSecurityState(
          GetActiveWebContents());
      ASSERT_TRUE(security_state);
      EXPECT_EQ(
          security_state->malicious_content_status,
          security_state::MALICIOUS_CONTENT_STATUS_WARNABLE_SUSPICIOUS_SITE);
    } else {
      EXPECT_FALSE(shown_future.IsReady());
      EXPECT_FALSE(
          safe_browsing::SuspiciousSiteControllerDesktop::FromWebContents(
              GetActiveWebContents()));
      EXPECT_FALSE(PageInfoBubbleViewBase::GetPageInfoBubbleForTesting());

      auto security_state = chrome_security_state::GetVisibleSecurityState(
          GetActiveWebContents());
      ASSERT_TRUE(security_state);
      EXPECT_NE(
          security_state->malicious_content_status,
          security_state::MALICIOUS_CONTENT_STATUS_WARNABLE_SUSPICIOUS_SITE);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  TestSafeBrowsingServiceFactory factory_;
};

IN_PROC_BROWSER_TEST_F(SuspiciousSiteDesktopBrowserTest,
                       ShowsWarningOnNavigation) {
  RunCheckWarningWithProtectionLevelTest(
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION,
      /*expect_warning=*/true);
}

IN_PROC_BROWSER_TEST_F(SuspiciousSiteDesktopBrowserTest, MarkAsSafeAllowsHost) {
  GURL malicious_url = embedded_test_server()->GetURL("/title1.html");
  SetURLThreatType(malicious_url,
                   SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);

  base::test::TestFuture<void> shown_future;
  SuspiciousSiteControllerDesktop::SetBubbleShownCallbackForTesting(
      shown_future.GetCallback());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), malicious_url));
  EXPECT_TRUE(shown_future.Wait());

  auto* bubble_view = static_cast<SuspiciousSiteBubbleView*>(
      PageInfoBubbleViewBase::GetPageInfoBubbleForTesting());
  ASSERT_TRUE(bubble_view);
  ASSERT_TRUE(bubble_view->GetWidget());
  EXPECT_TRUE(bubble_view->GetWidget()->IsVisible());
  EXPECT_TRUE(GetActiveWebContents()->ShouldIgnoreInputEventsForTesting());
  EXPECT_TRUE(browser()->GetTabStripModel()->IsTabBlocked(0));

  views::test::ButtonTestApi(bubble_view->mark_as_safe_button_for_testing())
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), base::TimeTicks(),
                                  ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON));

  HostContentSettingsMap* hcsm =
      HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile());
  EXPECT_TRUE(SuspiciousSiteWarningAllowlist(hcsm).IsSiteAllowedForHost(
      std::string(malicious_url.host())));

  EXPECT_FALSE(safe_browsing::SuspiciousSiteControllerDesktop::FromWebContents(
      GetActiveWebContents()));
  EXPECT_FALSE(GetActiveWebContents()->ShouldIgnoreInputEventsForTesting());
  EXPECT_FALSE(browser()->GetTabStripModel()->IsTabBlocked(0));

  auto security_state =
      chrome_security_state::GetVisibleSecurityState(GetActiveWebContents());
  ASSERT_TRUE(security_state);
  EXPECT_NE(security_state->malicious_content_status,
            security_state::MALICIOUS_CONTENT_STATUS_WARNABLE_SUSPICIOUS_SITE);
}

IN_PROC_BROWSER_TEST_F(SuspiciousSiteDesktopBrowserTest,
                       BackToSafetyNavigatesAway) {
  GURL safe_url = embedded_test_server()->GetURL("/title2.html");
  GURL malicious_url = embedded_test_server()->GetURL("/title1.html");
  SetURLThreatType(malicious_url,
                   SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), safe_url));

  base::test::TestFuture<void> shown_future;
  SuspiciousSiteControllerDesktop::SetBubbleShownCallbackForTesting(
      shown_future.GetCallback());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), malicious_url));
  EXPECT_TRUE(shown_future.Wait());

  auto* bubble_view = static_cast<SuspiciousSiteBubbleView*>(
      PageInfoBubbleViewBase::GetPageInfoBubbleForTesting());
  ASSERT_TRUE(bubble_view);
  ASSERT_TRUE(bubble_view->GetWidget());
  EXPECT_TRUE(bubble_view->GetWidget()->IsVisible());
  EXPECT_TRUE(GetActiveWebContents()->ShouldIgnoreInputEventsForTesting());
  EXPECT_TRUE(browser()->GetTabStripModel()->IsTabBlocked(0));

  views::test::ButtonTestApi(bubble_view->back_to_safety_button_for_testing())
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), base::TimeTicks(),
                                  ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_FALSE(safe_browsing::SuspiciousSiteControllerDesktop::FromWebContents(
      GetActiveWebContents()));
  EXPECT_FALSE(GetActiveWebContents()->ShouldIgnoreInputEventsForTesting());
  EXPECT_FALSE(browser()->GetTabStripModel()->IsTabBlocked(0));
}

IN_PROC_BROWSER_TEST_F(SuspiciousSiteDesktopBrowserTest,
                       DoesNotShowWarningInStandardProtection) {
  RunCheckWarningWithProtectionLevelTest(
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION,
      /*expect_warning=*/false);
}

IN_PROC_BROWSER_TEST_F(SuspiciousSiteDesktopBrowserTest,
                       DoesNotShowWarningInNoProtection) {
  RunCheckWarningWithProtectionLevelTest(
      safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING,
      /*expect_warning=*/false);
}

IN_PROC_BROWSER_TEST_F(SuspiciousSiteDesktopBrowserTest,
                       ShowsWarningWhenAsyncCheckCompletesAfterNavigation) {
  GURL malicious_url = embedded_test_server()->GetURL("/title1.html");
  SetURLThreatType(malicious_url,
                   SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);

  base::test::TestFuture<void> shown_future;
  SuspiciousSiteControllerDesktop::SetBubbleShownCallbackForTesting(
      shown_future.GetCallback());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), malicious_url));
  EXPECT_TRUE(shown_future.Wait());

  auto* controller =
      safe_browsing::SuspiciousSiteControllerDesktop::FromWebContents(
          GetActiveWebContents());
  ASSERT_TRUE(controller);

  // Trigger OnAsyncSafeBrowsingCheckCompleted callback.
  controller->OnAsyncSafeBrowsingCheckCompleted();

  views::BubbleDialogDelegateView* bubble =
      PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
  ASSERT_TRUE(bubble);
  ASSERT_TRUE(bubble->GetWidget());
  EXPECT_TRUE(bubble->GetWidget()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(SuspiciousSiteDesktopBrowserTest,
                       MarkAsSafeInPageInfoBubbleRemovesWarningChip) {
  GURL malicious_url = embedded_test_server()->GetURL("/title1.html");
  SetURLThreatType(malicious_url,
                   SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE);

  base::test::TestFuture<void> shown_future;
  SuspiciousSiteControllerDesktop::SetBubbleShownCallbackForTesting(
      shown_future.GetCallback());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), malicious_url));
  EXPECT_TRUE(shown_future.Wait());

  // Dismiss the initial warning bubble.
  auto* bubble_view = static_cast<SuspiciousSiteBubbleView*>(
      PageInfoBubbleViewBase::GetPageInfoBubbleForTesting());
  ASSERT_TRUE(bubble_view);
  bubble_view->GetWidget()->CloseNow();
  EXPECT_FALSE(PageInfoBubbleViewBase::GetPageInfoBubbleForTesting());

  // Suspicious security state and controller should still be active.
  EXPECT_TRUE(safe_browsing::SuspiciousSiteControllerDesktop::FromWebContents(
      GetActiveWebContents()));
  auto security_state =
      chrome_security_state::GetVisibleSecurityState(GetActiveWebContents());
  ASSERT_TRUE(security_state);
  EXPECT_EQ(security_state->malicious_content_status,
            security_state::MALICIOUS_CONTENT_STATUS_WARNABLE_SUSPICIOUS_SITE);

  // Open Page Info bubble via location icon.
  LocationIconTestAccessor(browser()).ShowBubble();
  views::BubbleDialogDelegateView* page_info =
      PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
  ASSERT_TRUE(page_info);

  views::View* mark_as_safe_btn =
      page_info->GetWidget()->GetRootView()->GetViewByID(
          PageInfoViewFactory::
              VIEW_ID_PAGE_INFO_SUSPICIOUS_SITE_MARK_AS_SAFE_BUTTON);
  ASSERT_TRUE(mark_as_safe_btn);

  views::test::ButtonTestApi(
      static_cast<views::MdTextButton*>(mark_as_safe_btn))
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), base::TimeTicks(),
                                  ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON));

  // Site should be allowed in settings.
  HostContentSettingsMap* hcsm =
      HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile());
  EXPECT_TRUE(SuspiciousSiteWarningAllowlist(hcsm).IsSiteAllowedForHost(
      std::string(malicious_url.host())));

  // Controller should be destroyed.
  EXPECT_FALSE(safe_browsing::SuspiciousSiteControllerDesktop::FromWebContents(
      GetActiveWebContents()));

  // Security state should immediately revert to normal without needing a page
  // refresh.
  security_state =
      chrome_security_state::GetVisibleSecurityState(GetActiveWebContents());
  ASSERT_TRUE(security_state);
  EXPECT_NE(security_state->malicious_content_status,
            security_state::MALICIOUS_CONTENT_STATUS_WARNABLE_SUSPICIOUS_SITE);
}

}  // namespace safe_browsing
