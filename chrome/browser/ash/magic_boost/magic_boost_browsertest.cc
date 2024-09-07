// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/magic_boost/magic_boost_constants.h"
#include "ash/system/magic_boost/magic_boost_disclaimer_view.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_panel_widget.h"
#include "ash/system/mahi/mahi_ui_update.h"
#include "ash/test/ash_test_util.h"
#include "base/command_line.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_mediator_factory.h"
#include "chrome/browser/ash/mahi/mahi_test_util.h"
#include "chrome/browser/ash/mahi/mahi_ui_browser_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_constants.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_opt_in_card.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_view.h"
#include "chrome/browser/ui/webui/ash/mako/mako_bubble_coordinator.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/mahi/public/cpp/mahi_switches.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_utils.h"

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

class MagicBoostBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<std::tuple<
          /*editor_mode=*/input_method::EditorMode,
          /*orca_consent_status=*/input_method::ConsentStatus,
          /*is_hmr_consent_unset=*/chromeos::HMRConsentStatus>> {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kMahi,
                              chromeos::features::kOrca,
                              chromeos::features::kFeatureManagementMahi,
                              chromeos::features::kFeatureManagementOrca},
        /*disabled_features=*/{});

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kMahiRestrictionsOverride);

    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

 protected:
  ui::test::EventGenerator& event_generator() { return *event_generator_; }

  // Navigates to the read only content test website and right click on it.
  void NavigateAndRightClickReadOnlyWeb() {
    NavigateToReadOnlyWeb();

    event_generator().MoveMouseTo(chrome_test_utils::GetActiveWebContents(this)
                                      ->GetViewBounds()
                                      .CenterPoint());
    event_generator().ClickRightButton();
  }

  // Navigates to the read only content test website.
  void NavigateToReadOnlyWeb() {
    // Waits until the page is ready.
    content::RenderFrameHost* render_frame_host = ui_test_utils::NavigateToURL(
        browser(), https_server_.GetURL("/mahi/test_article.html"));
    ASSERT_TRUE(render_frame_host);
    content::MainThreadFrameObserver(render_frame_host->GetRenderWidgetHost())
        .Wait();
  }

  // Navigates to the input view test website, selects the input text and right
  // clicks on it.
  void NavigateAndRightClickInputTextWeb() {
    // Waits until the page is ready.
    content::RenderFrameHost* render_frame_host = ui_test_utils::NavigateToURL(
        browser(), https_server_.GetURL("/magic_boost/test_input_box.html"));
    ASSERT_TRUE(render_frame_host);
    content::MainThreadFrameObserver(render_frame_host->GetRenderWidgetHost())
        .Wait();

    // Finds the position of the input field. The js function returns a list of
    // [L, T, R, B] of the input view.
    auto result =
        content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                        "getTextfieldBound();");
    ASSERT_TRUE(result.error.empty());
    auto value = result.ExtractList();
    ASSERT_TRUE(value.is_list());
    const base::Value::List bounds_as_list = std::move(value).TakeList();
    ASSERT_EQ(bounds_as_list.size(), 4u);
    const double left = bounds_as_list[0].GetDouble();
    const double top = bounds_as_list[1].GetDouble();
    const double right = bounds_as_list[2].GetDouble();
    const double bottom = bounds_as_list[3].GetDouble();

    gfx::Point textfield_in_screen = browser()
                                         ->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetContainerBounds()
                                         .origin();
    textfield_in_screen.Offset(left, top);
    const gfx::Rect view_bounds =
        gfx::Rect(textfield_in_screen.x(), textfield_in_screen.y(),
                  right - left, bottom - top);

    // Selects the input text and right click on it.
    event_generator().MoveMouseTo(view_bounds.left_center() +
                                  gfx::Vector2d(5, 0));
    event_generator().PressLeftButton();
    event_generator().MoveMouseTo(view_bounds.right_center() +
                                  gfx::Vector2d(-5, 0));
    event_generator().ReleaseLeftButton();
    event_generator().MoveMouseTo(view_bounds.CenterPoint());
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

  // Showing "chrome-untrusted://mako/" help me write bubble.
  bool IsShowingMakoBubble() const {
    return ash::input_method::EditorMediatorFactory::GetForProfile(
               browser()->profile())
        ->mako_bubble_coordinator_for_testing()
        .IsShowingUI();
  }

  bool ShouldIncludeOrca() const {
    // See `GetConsentStatusFromInteger` method in `editor_consent_enums.cc`,
    // `kInvalid` is treated as `kUnset`.
    return GetEditorMode() != input_method::EditorMode::kHardBlocked &&
           (GetInitEditorConsentStatus() ==
                input_method::ConsentStatus::kUnset ||
            GetInitEditorConsentStatus() ==
                input_method::ConsentStatus::kInvalid);
  }

  bool ShouldOptInHmr() const {
    return GetInitHmrConsentStatus() == chromeos::HMRConsentStatus::kUnset;
  }

  bool ShouldShowEditorMenu() const {
    // In production, when the editor is not soft/hard blocked, it checks the
    // Orca consent status to find the current editor mode. It will get
    // `kRewrite` when the selected length is greater than 0, and get `kWrite`
    // when the selected length is 0.
    return GetEditorMode() == input_method::EditorMode::kRewrite ||
           GetEditorMode() == input_method::EditorMode::kWrite;
  }

  input_method::EditorMode GetEditorMode() const {
    return std::get<0>(GetParam());
  }

  input_method::ConsentStatus GetInitEditorConsentStatus() const {
    return std::get<1>(GetParam());
  }

  chromeos::HMRConsentStatus GetInitHmrConsentStatus() const {
    return std::get<2>(GetParam());
  }

 private:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        Shell::GetPrimaryRootWindow());

    // Configure `https_server_` so that the test page is accessible.
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    // Sets the editor mode.
    input_method::EditorMediatorFactory::GetForProfile(browser()->profile())
        ->OverrideEditorModeForTesting(GetEditorMode());

    // Sets the Orca consent status.
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kOrcaConsentStatus,
        base::to_underlying(GetInitEditorConsentStatus()));

    // Sets the Hmr consent status.
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kHMRConsentStatus,
        base::to_underlying(GetInitHmrConsentStatus()));
  }

  base::test::ScopedFeatureList feature_list_;
  base::AutoReset<bool> ignore_mahi_secret_key_ =
      switches::SetIgnoreMahiSecretKeyForTest();
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  net::EmbeddedTestServer https_server_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    MagicBoostBrowserTest,
    testing::Combine(
        /*editor_mode=*/testing::Values(
            input_method::EditorMode::kHardBlocked,
            input_method::EditorMode::kSoftBlocked,
            input_method::EditorMode::kConsentNeeded,
            input_method::EditorMode::kRewrite,
            input_method::EditorMode::kWrite),
        /*orca_consent_status=*/
        testing::Values(input_method::ConsentStatus::kInvalid,
                        input_method::ConsentStatus::kPending,
                        input_method::ConsentStatus::kApproved,
                        input_method::ConsentStatus::kDeclined,
                        input_method::ConsentStatus::kUnset),
        /*hmr_consent_status=*/
        testing::Values(chromeos::HMRConsentStatus::kUnset,
                        chromeos::HMRConsentStatus::kApproved,
                        chromeos::HMRConsentStatus::kDeclined,
                        chromeos::HMRConsentStatus::kPendingDisclaimer)));

IN_PROC_BROWSER_TEST_P(MagicBoostBrowserTest, AcceptOptInFromReadOnlyContent) {
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  EXPECT_EQ(chromeos::MagicBoostState::Get()->hmr_consent_status(),
            GetInitHmrConsentStatus());
  EXPECT_TRUE(chromeos::MagicBoostState::Get()->hmr_enabled().value());
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kHmrEnabled));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kOrcaEnabled));
  EXPECT_EQ(prefs->GetInteger(prefs::kHMRConsentStatus),
            base::to_underlying(GetInitHmrConsentStatus()));
  EXPECT_EQ(prefs->GetInteger(prefs::kOrcaConsentStatus),
            base::to_underlying(GetInitEditorConsentStatus()));

  // Right click on the web content to show the opt in card.
  NavigateAndRightClickReadOnlyWeb();

  // Not showing the opt in flow if should not opt in hmr.
  if (!ShouldOptInHmr()) {
    EXPECT_FALSE(FindWidgetWithName(
        chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
    EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
    return;
  }

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

  if (ShouldIncludeOrca()) {
    EXPECT_TRUE(prefs->GetBoolean(prefs::kOrcaEnabled));
    EXPECT_EQ(prefs->GetInteger(prefs::kOrcaConsentStatus),
              base::to_underlying(input_method::ConsentStatus::kApproved));
  } else {
    EXPECT_TRUE(prefs->GetBoolean(prefs::kOrcaEnabled));
    EXPECT_EQ(prefs->GetInteger(prefs::kOrcaConsentStatus),
              base::to_underlying(GetInitEditorConsentStatus()));
  }

  // Not showing the Editor Menu when opt in from read only content.
  EXPECT_FALSE(IsShowingMakoBubble());

  // Right click on the web content again.
  NavigateAndRightClickReadOnlyWeb();

  // Cannot find the opt in card any more.
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
}

IN_PROC_BROWSER_TEST_P(MagicBoostBrowserTest,
                       DeclineThroughCardFromReadOnlyContent) {
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  EXPECT_EQ(chromeos::MagicBoostState::Get()->hmr_consent_status(),
            GetInitHmrConsentStatus());
  EXPECT_EQ(chromeos::MagicBoostState::Get()->hmr_enabled().value(), true);
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kHmrEnabled));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kOrcaEnabled));
  EXPECT_EQ(prefs->GetInteger(prefs::kHMRConsentStatus),
            base::to_underlying(GetInitHmrConsentStatus()));
  EXPECT_EQ(prefs->GetInteger(prefs::kOrcaConsentStatus),
            base::to_underlying(GetInitEditorConsentStatus()));

  // Right click on the web content to show the opt in card.
  NavigateAndRightClickReadOnlyWeb();

  // Not showing the opt in flow if should not opt in hmr.
  if (!ShouldOptInHmr()) {
    EXPECT_FALSE(FindWidgetWithName(
        chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
    EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
    return;
  }

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

  if (ShouldIncludeOrca()) {
    EXPECT_FALSE(prefs->GetBoolean(prefs::kOrcaEnabled));
    EXPECT_EQ(prefs->GetInteger(prefs::kOrcaConsentStatus),
              base::to_underlying(input_method::ConsentStatus::kDeclined));
  } else {
    EXPECT_TRUE(prefs->GetBoolean(prefs::kOrcaEnabled));
    EXPECT_EQ(prefs->GetInteger(prefs::kOrcaConsentStatus),
              base::to_underlying(GetInitEditorConsentStatus()));
  }

  // Not showing the Editor Menu when opt in from read only content.
  EXPECT_FALSE(IsShowingMakoBubble());

  // Right click on the web content again.
  NavigateAndRightClickReadOnlyWeb();

  // Cannot find the opt in card any more.
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
}

IN_PROC_BROWSER_TEST_P(MagicBoostBrowserTest,
                       DeclineThroughDisclaimerViewFromReadOnlyContent) {
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  EXPECT_EQ(chromeos::MagicBoostState::Get()->hmr_consent_status(),
            GetInitHmrConsentStatus());
  EXPECT_TRUE(chromeos::MagicBoostState::Get()->hmr_enabled().value());
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kHmrEnabled));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kOrcaEnabled));
  EXPECT_EQ(prefs->GetInteger(prefs::kHMRConsentStatus),
            base::to_underlying(GetInitHmrConsentStatus()));
  EXPECT_EQ(prefs->GetInteger(prefs::kOrcaConsentStatus),
            base::to_underlying(GetInitEditorConsentStatus()));

  // Right click on the web content to show the opt in card.
  NavigateAndRightClickReadOnlyWeb();

  // Not showing the opt in flow if should not opt in hmr.
  if (!ShouldOptInHmr()) {
    EXPECT_FALSE(FindWidgetWithName(
        chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
    EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
    return;
  }

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

  if (ShouldIncludeOrca()) {
    EXPECT_FALSE(prefs->GetBoolean(prefs::kOrcaEnabled));
    EXPECT_EQ(prefs->GetInteger(prefs::kOrcaConsentStatus),
              base::to_underlying(input_method::ConsentStatus::kDeclined));
  } else {
    EXPECT_TRUE(prefs->GetBoolean(prefs::kOrcaEnabled));
    EXPECT_EQ(prefs->GetInteger(prefs::kOrcaConsentStatus),
              base::to_underlying(GetInitEditorConsentStatus()));
  }

  // Not showing the Editor Menu when opt in from read only content.
  EXPECT_FALSE(IsShowingMakoBubble());

  // Right click on the web content again.
  NavigateAndRightClickReadOnlyWeb();

  // Cannot find the opt in card any more.
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
}

IN_PROC_BROWSER_TEST_P(MagicBoostBrowserTest, FindNothingOnBlankWebPage) {
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

IN_PROC_BROWSER_TEST_P(MagicBoostBrowserTest, AcceptOptInFromInputFieldWeb) {
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  EXPECT_EQ(chromeos::MagicBoostState::Get()->hmr_consent_status(),
            GetInitHmrConsentStatus());
  EXPECT_TRUE(chromeos::MagicBoostState::Get()->hmr_enabled().value());
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kHmrEnabled));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kOrcaEnabled));
  EXPECT_EQ(prefs->GetInteger(prefs::kHMRConsentStatus),
            base::to_underlying(GetInitHmrConsentStatus()));
  EXPECT_EQ(prefs->GetInteger(prefs::kOrcaConsentStatus),
            base::to_underlying(GetInitEditorConsentStatus()));

  // Right click on the input.
  NavigateAndRightClickInputTextWeb();

  // If should not include orca, there's no opt in flow from the input text
  // page.
  if (!ShouldIncludeOrca()) {
    EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
    EXPECT_FALSE(FindWidgetWithName(
        chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
    return;
  }

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

  // Closes the disclaimer view and checks the corresponding prefs. No matter
  // what is the init Hmr status it will opt in Hmr again with the Orca feature,
  // but in production it is expected that Hmr status will be unset when Orca is
  // unset, since Hmr is launched after Orca.
  WaitUntilViewClosed(disclaimer_view_widget);
  EXPECT_EQ(chromeos::MagicBoostState::Get()->hmr_consent_status(),
            chromeos::HMRConsentStatus::kApproved);
  EXPECT_TRUE(chromeos::MagicBoostState::Get()->hmr_enabled().value());
  EXPECT_TRUE(prefs->GetBoolean(prefs::kHmrEnabled));
  EXPECT_EQ(prefs->GetInteger(prefs::kHMRConsentStatus),
            base::to_underlying(chromeos::HMRConsentStatus::kApproved));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kOrcaEnabled));
  EXPECT_EQ(prefs->GetInteger(prefs::kOrcaConsentStatus),
            base::to_underlying(input_method::ConsentStatus::kApproved));

  // Shows the Editor Menu if the editor mode is not (soft/hard) blocked.
  if (ShouldShowEditorMenu()) {
    EXPECT_TRUE(IsShowingMakoBubble());
  } else {
    EXPECT_FALSE(IsShowingMakoBubble());
  }

  // Right click on the input again.
  NavigateAndRightClickInputTextWeb();

  // Cannot find the opt in card any more.
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
}

IN_PROC_BROWSER_TEST_P(MagicBoostBrowserTest,
                       DeclineThroughCardFromInputFieldWeb) {
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  EXPECT_EQ(chromeos::MagicBoostState::Get()->hmr_consent_status(),
            GetInitHmrConsentStatus());
  EXPECT_TRUE(chromeos::MagicBoostState::Get()->hmr_enabled().value());
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kHmrEnabled));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kOrcaEnabled));
  EXPECT_EQ(prefs->GetInteger(prefs::kHMRConsentStatus),
            base::to_underlying(GetInitHmrConsentStatus()));
  EXPECT_EQ(prefs->GetInteger(prefs::kOrcaConsentStatus),
            base::to_underlying(GetInitEditorConsentStatus()));

  // Right click on the input.
  NavigateAndRightClickInputTextWeb();

  // If should not include orca, there's no opt in flow from the input text
  // page.
  if (!ShouldIncludeOrca()) {
    EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
    EXPECT_FALSE(FindWidgetWithName(
        chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
    return;
  }

  // Finds the opt in card and still cannot find the disclaimer view.
  views::Widget* opt_in_card_widget = GetOptInCardWidget();
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  ASSERT_TRUE(opt_in_card_widget);

  // Left click on the decline button in the opt in card.
  LeftClickOnView(GetOptInCardDeclineButton());

  // Closes the opt in card and checks the corresponding prefs. Not showing the
  // disclaimer view.
  WaitUntilViewClosed(opt_in_card_widget);
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));

  // Checks the corresponding prefs. No matter what is the init Hmr status it
  // will opt in Hmr again with the Orca feature, but in production it is
  // expected that Hmr status will be unset when Orca is unset, since Hmr is
  // launched after Orca.
  EXPECT_EQ(chromeos::MagicBoostState::Get()->hmr_consent_status(),
            chromeos::HMRConsentStatus::kDeclined);
  EXPECT_FALSE(chromeos::MagicBoostState::Get()->hmr_enabled().value());
  EXPECT_FALSE(prefs->GetBoolean(prefs::kHmrEnabled));
  EXPECT_EQ(prefs->GetInteger(prefs::kHMRConsentStatus),
            base::to_underlying(chromeos::HMRConsentStatus::kDeclined));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kOrcaEnabled));
  EXPECT_EQ(prefs->GetInteger(prefs::kOrcaConsentStatus),
            base::to_underlying(input_method::ConsentStatus::kDeclined));

  // Not showing the Editor Menu after declined.
  EXPECT_FALSE(IsShowingMakoBubble());

  // Right click on the input again.
  NavigateAndRightClickInputTextWeb();

  // Cannot find the opt in card any more.
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
}

IN_PROC_BROWSER_TEST_P(MagicBoostBrowserTest,
                       DeclineThroughDisclaimerViewFromInputFieldWeb) {
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  EXPECT_EQ(chromeos::MagicBoostState::Get()->hmr_consent_status(),
            GetInitHmrConsentStatus());
  EXPECT_TRUE(chromeos::MagicBoostState::Get()->hmr_enabled().value());
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kHmrEnabled));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kOrcaEnabled));
  EXPECT_EQ(prefs->GetInteger(prefs::kHMRConsentStatus),
            base::to_underlying(GetInitHmrConsentStatus()));
  EXPECT_EQ(prefs->GetInteger(prefs::kOrcaConsentStatus),
            base::to_underlying(GetInitEditorConsentStatus()));

  // Right click on the input.
  NavigateAndRightClickInputTextWeb();

  // If should not include orca, there's no opt in flow from the input text
  // page.
  if (!ShouldIncludeOrca()) {
    EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
    EXPECT_FALSE(FindWidgetWithName(
        chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
    return;
  }

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

  // Closes the disclaimer view and checks the corresponding prefs. No matter
  // what is the init Hmr status it will opt in Hmr again with the Orca feature,
  // but in production it is expected that Hmr status will be unset when Orca is
  // unset, since Hmr is launched after Orca.
  WaitUntilViewClosed(disclaimer_view_widget);
  EXPECT_EQ(chromeos::MagicBoostState::Get()->hmr_consent_status(),
            chromeos::HMRConsentStatus::kDeclined);
  EXPECT_FALSE(chromeos::MagicBoostState::Get()->hmr_enabled().value());
  EXPECT_FALSE(prefs->GetBoolean(prefs::kHmrEnabled));
  EXPECT_EQ(prefs->GetInteger(prefs::kHMRConsentStatus),
            base::to_underlying(chromeos::HMRConsentStatus::kDeclined));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kOrcaEnabled));
  EXPECT_EQ(prefs->GetInteger(prefs::kOrcaConsentStatus),
            base::to_underlying(input_method::ConsentStatus::kDeclined));

  // Not showing the Editor Menu after declined.
  EXPECT_FALSE(IsShowingMakoBubble());

  // Right click on the input again.
  NavigateAndRightClickInputTextWeb();

  // Cannot find the opt in card any more.
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
}

IN_PROC_BROWSER_TEST_P(MagicBoostBrowserTest, ShowDisclaimerViewOnMultiScreen) {
  // Cache the init hmr status, which will be used to reset the status for
  // testing showing the disclaimer view on different screens. Without resetting
  // the status, it will not show the disclaimer view again and show the mahi
  // menu instead.
  auto init_hmr_status = GetInitHmrConsentStatus();

  // Creates 3 displays.
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .UpdateDisplay("1000x700,1100x800,1200x700");
  auto root_windows = ash::Shell::GetAllRootWindows();
  ASSERT_EQ(3u, root_windows.size());
  ASSERT_EQ(ash::Shell::GetPrimaryRootWindow(), root_windows[0]);

  // Can not find any widget.
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));

  display::Displays displays =
      Shell::Get()->display_manager()->active_display_list();

  // Sets the second display to be the window screen. Right click on the web
  // content to show the opt in card.
  browser()->window()->SetBounds(displays[1].work_area());
  event_generator().SetTargetWindow(root_windows[1]);
  NavigateToReadOnlyWeb();
  event_generator().MoveMouseTo(displays[1].work_area().CenterPoint());
  event_generator().ClickRightButton();

  // Not showing the opt in flow if should not opt in hmr.
  if (!ShouldOptInHmr()) {
    EXPECT_FALSE(FindWidgetWithName(
        chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
    EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
    return;
  }

  // Finds the opt in card and still cannot find the disclaimer view.
  views::Widget* opt_in_card_widget = GetOptInCardWidget();
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  ASSERT_TRUE(opt_in_card_widget);

  // Left click on the accept button in the opt in card.
  LeftClickOnView(GetOptInCardAcceptButton());

  // Closes the opt in card and shows the disclaimer view on the second screen.
  WaitUntilViewClosed(opt_in_card_widget);
  EXPECT_TRUE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  ASSERT_EQ(root_windows[1], views::GetRootWindow(GetDisclaimerViewWidget()));

  // Resets the Hmr consent status to continue testing showing disclaimer view
  // on the first screen.
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kHMRConsentStatus, base::to_underlying(init_hmr_status));
  browser()->window()->SetBounds(displays[0].work_area());
  event_generator().SetTargetWindow(root_windows[0]);
  NavigateToReadOnlyWeb();
  event_generator().MoveMouseTo(displays[0].work_area().CenterPoint());
  event_generator().ClickRightButton();

  // Left click on the accept button in the opt in card.
  ASSERT_TRUE(GetOptInCardWidget());
  LeftClickOnView(GetOptInCardAcceptButton());

  // Closes the opt in card and shows the disclaimer view on the first screen.
  WaitUntilViewClosed(GetOptInCardWidget());
  EXPECT_TRUE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  ASSERT_EQ(root_windows[0], views::GetRootWindow(GetDisclaimerViewWidget()));

  // Resets the Hmr consent status to continue testing showing disclaimer view
  // on the third screen.
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kHMRConsentStatus, base::to_underlying(init_hmr_status));
  browser()->window()->SetBounds(displays[2].work_area());
  event_generator().SetTargetWindow(root_windows[2]);
  NavigateToReadOnlyWeb();
  event_generator().MoveMouseTo(displays[2].work_area().CenterPoint());
  event_generator().ClickRightButton();

  // Left click on the accept button in the opt in card.
  ASSERT_TRUE(GetOptInCardWidget());
  LeftClickOnView(GetOptInCardAcceptButton());

  // Closes the opt in card and shows the disclaimer view on the third screen.
  WaitUntilViewClosed(GetOptInCardWidget());
  EXPECT_TRUE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
  ASSERT_EQ(root_windows[2], views::GetRootWindow(GetDisclaimerViewWidget()));

  // Without resetting the hmr consent status, it should show mahi menu and
  // close the dicaimer view after right clicking on the read only web content
  // again on the first screen.
  browser()->window()->SetBounds(displays[0].work_area());
  event_generator().SetTargetWindow(root_windows[0]);
  NavigateToReadOnlyWeb();
  event_generator().MoveMouseTo(displays[0].work_area().CenterPoint());
  event_generator().ClickRightButton();

  // Finds the mahi menu. Can not find the opt in card or disclaimer view any
  // more.
  views::Widget* mahi_widget = FindWidgetWithNameAndWaitIfNeeded(
      chromeos::mahi::MahiMenuView::GetWidgetName());
  ASSERT_TRUE(mahi_widget);
  EXPECT_FALSE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));
  EXPECT_FALSE(FindWidgetWithName(
      chromeos::MagicBoostOptInCard::GetWidgetNameForTest()));
}

// MahiUiWithOptInViewBrowserTest ----------------------------------------------

class MahiUiWithOptInCardBrowserTest
    : public MahiUiBrowserTestBase,
      public ::testing::WithParamInterface</*accept=*/bool> {
 private:
  // MahiUiBrowserTestBase:
  void SetUp() override {
    // Enable Orca to ensure the existence of the write editor controller which
    // is required to show the opt-in card.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kFeatureManagementOrca,
                              chromeos::features::kFeatureManagementMahi,
                              chromeos::features::kMahi,
                              chromeos::features::kOrca},
        /*disabled_features=*/{});

    MahiUiBrowserTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    MahiUiBrowserTestBase::SetUpOnMainThread();
    ApplyHMRConsentStatusAndWait(chromeos::HMRConsentStatus::kUnset);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         MahiUiWithOptInCardBrowserTest,
                         /*accept=*/::testing::Bool());

// Verifies Mahi UI features by accepting or declining the disclaimer view that
// is launched by the opt-in flow.
IN_PROC_BROWSER_TEST_P(MahiUiWithOptInCardBrowserTest, Basics) {
  EXPECT_FALSE(
      FindWidgetWithName(chromeos::MagicBoostOptInCard::GetWidgetName()));

  ui::ScopedAnimationDurationScaleMode zero_duration(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // Open the opt-in card by mouse right click on the web contents.
  event_generator().MoveMouseTo(chrome_test_utils::GetActiveWebContents(this)
                                    ->GetViewBounds()
                                    .CenterPoint());
  event_generator().ClickRightButton();
  views::Widget* const opt_in_card_widget = FindWidgetWithNameAndWaitIfNeeded(
      chromeos::MagicBoostOptInCard::GetWidgetName());
  ASSERT_TRUE(opt_in_card_widget);

  const views::View* const opt_in_button =
      opt_in_card_widget->GetContentsView()->GetViewByID(
          chromeos::magic_boost::OptInCardPrimaryButton);

  // Show the disclaimer view by clicking the `opt_in_button`.
  ASSERT_TRUE(opt_in_button);
  event_generator().MoveMouseTo(
      opt_in_button->GetBoundsInScreen().CenterPoint());
  event_generator().ClickLeftButton();

  WaitUntilViewClosed(opt_in_card_widget);
  EXPECT_TRUE(FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName()));

  const bool accept = GetParam();
  ClickDisclaimerViewButton(accept);

  // If user clicks the declination button, the Mahi panel should not show.
  if (!accept) {
    EXPECT_FALSE(FindWidgetWithName(MahiPanelWidget::GetName()));
    return;
  }

  // The code below checks the Mahi panel.

  WaitUntilUiUpdateReceived(MahiUiUpdateType::kSummaryLoaded);
  views::Widget* panel_widget =
      FindWidgetWithNameAndWaitIfNeeded(MahiPanelWidget::GetName());
  ASSERT_TRUE(panel_widget);

  const auto* const summary_label = views::AsViewClass<views::Label>(
      panel_widget->GetContentsView()->GetViewByID(
          mahi_constants::ViewId::kSummaryLabel));
  ASSERT_TRUE(summary_label);
  EXPECT_EQ(base::UTF16ToUTF8(summary_label->GetText()),
            GetMahiDefaultTestSummary());
}

}  // namespace ash
