// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/magic_boost/magic_boost_constants.h"
#include "ash/system/magic_boost/magic_boost_disclaimer_view.h"
#include "ash/test/ash_test_util.h"
#include "base/command_line.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_constants.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_opt_in_card.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/mahi/public/cpp/mahi_switches.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Runs a specific callback when the observed view is deleted.
class ViewDeletionObserver : public ::views::ViewObserver {
 public:
  ViewDeletionObserver(views::View* view,
                       base::RepeatingClosure on_delete_callback)
      : on_delete_callback_(on_delete_callback) {
    observation_.Observe(view);
  }

 private:
  // ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override {
    observation_.Reset();
    on_delete_callback_.Run();
  }

  base::RepeatingClosure on_delete_callback_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

// Waits until the content view specified by `widget` is closed.
void WaitUntilViewClosed(views::Widget* widget) {
  ASSERT_TRUE(widget);

  base::RunLoop run_loop;
  ViewDeletionObserver view_observer(
      widget->GetContentsView(),
      base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();
}

}  // namespace

class MagicBoostBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kMahi,
                              chromeos::features::kMagicBoost,
                              chromeos::features::kOrca,
                              chromeos::features::kFeatureManagementOrca},
        /*disabled_features=*/{});

    InProcessBrowserTest::SetUp();
  }

 protected:
  ui::test::EventGenerator& event_generator() { return *event_generator_; }

  // Navigates to the read only content test website and right click on it.
  void NavigateAndRightClickReadOnlyWeb() {
    // Waits until the page is ready.
    content::RenderFrameHost* render_frame_host = ui_test_utils::NavigateToURL(
        browser(), https_server_.GetURL("/mahi/test_article.html"));
    ASSERT_TRUE(render_frame_host);
    content::MainThreadFrameObserver(render_frame_host->GetRenderWidgetHost())
        .Wait();

    event_generator().MoveMouseTo(chrome_test_utils::GetActiveWebContents(this)
                                      ->GetViewBounds()
                                      .CenterPoint());
    event_generator().ClickRightButton();
  }

  void LeftClickOnView(const views::View* view) {
    ASSERT_TRUE(view);
    event_generator().MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
    event_generator().ClickLeftButton();
  }

  views::Widget* GetOptInCardWidget() const {
    return FindWidgetWithNameAndWaitIfNeeded(
        chromeos::MagicBoostOptInCard::GetWidgetNameForTest());
  }

  views::View* GetOptInCardAcceptButton() const {
    return GetOptInCardWidget()->GetContentsView()->GetViewByID(
        chromeos::magic_boost::ViewId::OptInCardPrimaryButton);
  }

  views::View* GetOptInCardDeclineButton() const {
    return GetOptInCardWidget()->GetContentsView()->GetViewByID(
        chromeos::magic_boost::ViewId::OptInCardSecondaryButton);
  }

  views::Widget* GetDisclaimerViewWidget() const {
    return FindWidgetWithNameAndWaitIfNeeded(
        MagicBoostDisclaimerView::GetWidgetName());
  }

  views::View* GetDisclaimerViewAcceptButton() const {
    return GetDisclaimerViewWidget()->GetContentsView()->GetViewByID(
        magic_boost::ViewId::DisclaimerViewAcceptButton);
  }

  views::View* GetDisclaimerViewDeclineButton() const {
    return GetDisclaimerViewWidget()->GetContentsView()->GetViewByID(
        magic_boost::ViewId::DisclaimerViewDeclineButton);
  }

 private:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        Shell::GetPrimaryRootWindow());

    // Configure `https_server_` so that the test page is accessible.
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
  }

  base::test::ScopedFeatureList feature_list_;
  base::AutoReset<bool> ignore_mahi_secret_key_ =
      switches::SetIgnoreMahiSecretKeyForTest();
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  net::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(MagicBoostBrowserTest, AcceptOptInHmrOnly) {
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  EXPECT_EQ(chromeos::MagicBoostState::Get()->hmr_consent_status(),
            chromeos::HMRConsentStatus::kUnset);
  EXPECT_TRUE(chromeos::MagicBoostState::Get()->hmr_enabled().value());
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kHmrEnabled));
  EXPECT_EQ(prefs->GetInteger(prefs::kHMRConsentStatus),
            base::to_underlying(chromeos::HMRConsentStatus::kUnset));

  // Right click on the web content to show the opt in card.
  NavigateAndRightClickReadOnlyWeb();

  // Finds the opt in card and still cannot find the disclaimer view.
  views::Widget* opt_in_card_widget = GetOptInCardWidget();
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  ASSERT_TRUE(opt_in_card_widget);

  // Left click on the accept button in the opt in card.
  LeftClickOnView(GetOptInCardAcceptButton());

  // Closes the opt in card and shows the disclaimer view.
  WaitUntilViewClosed(opt_in_card_widget);
  EXPECT_TRUE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  views::Widget* disclaimer_view_widget = GetDisclaimerViewWidget();
  ASSERT_TRUE(disclaimer_view_widget);

  // Left click on the accept button in the disclaimer view.
  LeftClickOnView(GetDisclaimerViewAcceptButton());

  // Closes the disclaimer view and checks the corresponding prefs.
  WaitUntilViewClosed(disclaimer_view_widget);
  EXPECT_EQ(chromeos::MagicBoostState::Get()->hmr_consent_status(),
            chromeos::HMRConsentStatus::kApproved);
  EXPECT_TRUE(chromeos::MagicBoostState::Get()->hmr_enabled().value());
  EXPECT_TRUE(prefs->GetBoolean(prefs::kHmrEnabled));
  EXPECT_EQ(prefs->GetInteger(prefs::kHMRConsentStatus),
            base::to_underlying(chromeos::HMRConsentStatus::kApproved));

  // Right click on the web content again.
  NavigateAndRightClickReadOnlyWeb();

  // Cannot find the opt in card any more.
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
}

IN_PROC_BROWSER_TEST_F(MagicBoostBrowserTest, DeclineThroughOptInCardHmrOnly) {
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  EXPECT_EQ(chromeos::MagicBoostState::Get()->hmr_consent_status(),
            chromeos::HMRConsentStatus::kUnset);
  EXPECT_EQ(chromeos::MagicBoostState::Get()->hmr_enabled().value(), true);
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kOrcaEnabled));
  EXPECT_EQ(prefs->GetInteger(prefs::kHMRConsentStatus),
            base::to_underlying(chromeos::HMRConsentStatus::kUnset));

  // Right click on the web content to show the opt in card.
  NavigateAndRightClickReadOnlyWeb();

  // Finds the opt in card and still cannot find the disclaimer view.
  views::Widget* opt_in_card_widget = GetOptInCardWidget();
  ASSERT_TRUE(opt_in_card_widget);
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));

  // Left click on the decline button in the opt in card.
  LeftClickOnView(GetOptInCardDeclineButton());

  // Closes the opt in card and checks the corresponding prefs. Not showing the
  // disclaimer view.
  WaitUntilViewClosed(opt_in_card_widget);
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));

  EXPECT_EQ(chromeos::MagicBoostState::Get()->hmr_consent_status(),
            chromeos::HMRConsentStatus::kDeclined);
  EXPECT_FALSE(chromeos::MagicBoostState::Get()->hmr_enabled().value());
  EXPECT_FALSE(prefs->GetBoolean(prefs::kHmrEnabled));
  EXPECT_EQ(prefs->GetInteger(prefs::kHMRConsentStatus),
            base::to_underlying(chromeos::HMRConsentStatus::kDeclined));

  // Right click on the web content again.
  NavigateAndRightClickReadOnlyWeb();

  // Cannot find the opt in card any more.
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
}

IN_PROC_BROWSER_TEST_F(MagicBoostBrowserTest,
                       DeclineThroughDisclaimerViewHmrOnly) {
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  EXPECT_EQ(chromeos::MagicBoostState::Get()->hmr_consent_status(),
            chromeos::HMRConsentStatus::kUnset);
  EXPECT_TRUE(chromeos::MagicBoostState::Get()->hmr_enabled().value());
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kHmrEnabled));
  EXPECT_EQ(prefs->GetInteger(prefs::kHMRConsentStatus),
            base::to_underlying(chromeos::HMRConsentStatus::kUnset));

  // Right click on the web content to show the opt in card.
  NavigateAndRightClickReadOnlyWeb();

  // Finds the opt in card and still cannot find the disclaimer view.
  views::Widget* opt_in_card_widget = GetOptInCardWidget();
  ASSERT_TRUE(opt_in_card_widget);
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));

  // Left click on the accept button in the opt in card.
  LeftClickOnView(GetOptInCardAcceptButton());

  // Closes the opt in card and shows the disclaimer view.
  WaitUntilViewClosed(opt_in_card_widget);
  EXPECT_TRUE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  views::Widget* disclaimer_view_widget = GetDisclaimerViewWidget();
  ASSERT_TRUE(disclaimer_view_widget);
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));

  // Left click on the decline button in the disclaimer view.
  LeftClickOnView(GetDisclaimerViewDeclineButton());

  // Closes the disclaimer view and checks the corresponding prefs.
  WaitUntilViewClosed(disclaimer_view_widget);
  EXPECT_EQ(chromeos::MagicBoostState::Get()->hmr_consent_status(),
            chromeos::HMRConsentStatus::kDeclined);
  EXPECT_FALSE(chromeos::MagicBoostState::Get()->hmr_enabled().value());
  EXPECT_FALSE(prefs->GetBoolean(prefs::kHmrEnabled));
  EXPECT_EQ(prefs->GetInteger(prefs::kHMRConsentStatus),
            base::to_underlying(chromeos::HMRConsentStatus::kDeclined));

  // Right click on the web content again.
  NavigateAndRightClickReadOnlyWeb();

  // Cannot find the opt in card any more.
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
}

IN_PROC_BROWSER_TEST_F(MagicBoostBrowserTest, FindNothingOnBlankWebPage) {
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));

  // Right click on a blank web page.
  event_generator().MoveMouseTo(chrome_test_utils::GetActiveWebContents(this)
                                    ->GetViewBounds()
                                    .CenterPoint());
  event_generator().ClickRightButton();

  // Cannot find the opt in card and the disclaimer view.
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
}

}  // namespace ash
