// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_onboarding_tooltip_controller.h"

#include <memory>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_panel_controller.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_onboarding_bubble_view.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/prefs.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/interaction/element_events.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace contextual_tasks {

namespace {

constexpr ui::ElementContext kTestContext =
    ui::ElementContext::CreateFakeContextForTesting(1);

std::unique_ptr<KeyedService> BuildMockTracker(content::BrowserContext*) {
  return std::make_unique<
      testing::NiceMock<feature_engagement::test::MockTracker>>();
}

}  // namespace

class ContextualTasksOnboardingTooltipControllerTest
    : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    anchor_widget_->Show();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{kContextualTasksSidePanelRearchitecture,
                              kContextualTasksShowOnboardingTooltip,
                              omnibox::kWebUIOmniboxAskGAboutThisPage},
        /*disabled_features=*/{kContextualTasksBypassDismissedCap});
    TestingProfile::Builder builder;
    builder.AddTestingFactory(feature_engagement::TrackerFactory::GetInstance(),
                              base::BindRepeating(&BuildMockTracker));
    profile_ = builder.Build();
    ON_CALL(mock_browser_window_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));
    controller_ = std::make_unique<ContextualTasksOnboardingTooltipController>(
        &mock_browser_window_, profile_.get());
    controller_->SetContextOverrideForTesting(kTestContext);
    controller_->SetHasAutomaticActiveTabForTesting(true);
  }

  void TearDown() override {
    test_element_.reset();
    controller_.reset();
    anchor_widget_.reset();
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

  TestingProfile* profile() { return profile_.get(); }
  ContextualTasksOnboardingTooltipController* controller() {
    return controller_.get();
  }

  void CreateAndShowFaviconsElement() {
    test_element_ = std::make_unique<ui::test::TestElement>(
        kContextualTasksFaviconsElementId, kTestContext);
    test_element_->SetNativeView(anchor_widget_->GetNativeView());
    test_element_->SetScreenBounds(gfx::Rect(100, 100, 50, 50));
    test_element_->Show();
  }

  void HideFaviconsElement() {
    if (test_element_) {
      test_element_->Hide();
    }
  }

  feature_engagement::test::MockTracker* mock_tracker() {
    return static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForBrowserContext(
            profile_.get()));
  }

 protected:
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<views::Widget> anchor_widget_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_;
  std::unique_ptr<ContextualTasksOnboardingTooltipController> controller_;
  std::unique_ptr<ui::test::TestElement> test_element_;
};

TEST_F(ContextualTasksOnboardingTooltipControllerTest,
       ElementShownShowsBubble) {
  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
  EXPECT_FALSE(controller()->IsBubbleShowing());
  CreateAndShowFaviconsElement();
  EXPECT_TRUE(controller()->IsBubbleShowing());
  EXPECT_EQ(controller()->active_variant_for_testing(),
            ContextualTasksOnboardingBubbleView::Variant::kOnboarding);
}

TEST_F(ContextualTasksOnboardingTooltipControllerTest,
       ElementWithoutWidgetDoesNotCrashOrShowBubbleUntilWidgetAttached) {
  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
  test_element_ = std::make_unique<ui::test::TestElement>(
      kContextualTasksFaviconsElementId, kTestContext);
  test_element_->SetScreenBounds(gfx::Rect(100, 100, 50, 50));
  test_element_->Show();

  // When the anchor element is shown before its WebContents native view is
  // attached to a Widget, MaybeShowBubble must not crash or create the bubble.
  EXPECT_FALSE(controller()->IsBubbleShowing());

  // Re-triggering via SetEntryPoint while still unattached also does not crash.
  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_OMNIBOX_ACTION);
  EXPECT_FALSE(controller()->IsBubbleShowing());

  // Once the element is backed by a Widget and shown, the bubble appears.
  test_element_->Hide();
  test_element_->SetNativeView(anchor_widget_->GetNativeView());
  test_element_->Show();
  EXPECT_TRUE(controller()->IsBubbleShowing());
  EXPECT_EQ(controller()->active_variant_for_testing(),
            ContextualTasksOnboardingBubbleView::Variant::kAskGoogle);
}

TEST_F(ContextualTasksOnboardingTooltipControllerTest,
       ElementBoundsChangedRepositionsBubble) {
  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
  CreateAndShowFaviconsElement();
  ASSERT_TRUE(controller()->IsBubbleShowing());
  const gfx::Rect initial_bounds =
      controller()->bubble_widget_for_testing()->GetWindowBoundsInScreen();

  test_element_->SetScreenBounds(gfx::Rect(220, 180, 50, 50));
  test_element_->SendCustomEvent(ui::kElementBoundsChangedEvent);

  EXPECT_NE(
      controller()->bubble_widget_for_testing()->GetWindowBoundsInScreen(),
      initial_bounds);
}

TEST_F(ContextualTasksOnboardingTooltipControllerTest,
       PreExistingElementShowsOnSetEntryPointAndLearnMoreOpensHelpTab) {
  CreateAndShowFaviconsElement();
  EXPECT_FALSE(controller()->IsBubbleShowing());

  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
  ASSERT_TRUE(controller()->IsBubbleShowing());

  auto* bubble_view = views::AsViewClass<ContextualTasksOnboardingBubbleView>(
      controller()
          ->bubble_widget_for_testing()
          ->widget_delegate()
          ->GetContentsView());
  ASSERT_NE(bubble_view, nullptr);
  EXPECT_CALL(mock_browser_window_,
              OpenGURL(GURL(GetContextualTasksOnboardingTooltipHelpUrl()),
                       ui::mojom::WindowOpenDisposition::kNewForegroundTab))
      .Times(1);
  bubble_view->body_label_for_testing()->ClickFirstLinkForTesting();
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                kContextualTasksOnboardingTooltipDismissedCount),
            0);
}

TEST_F(ContextualTasksOnboardingTooltipControllerTest,
       ElementHiddenClosesBubble) {
  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
  CreateAndShowFaviconsElement();
  EXPECT_TRUE(controller()->IsBubbleShowing());
  HideFaviconsElement();
  EXPECT_FALSE(controller()->IsBubbleShowing());
  EXPECT_FALSE(controller()->active_variant_for_testing().has_value());
}

TEST_F(ContextualTasksOnboardingTooltipControllerTest,
       AskGoogleVariantAcceptPrefsAndCapsRespected) {
  profile()->GetPrefs()->SetInteger(kContextualTasksSessionCountPostOnboarding,
                                    5);
  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_OMNIBOX_ACTION);
  CreateAndShowFaviconsElement();
  ASSERT_TRUE(controller()->IsBubbleShowing());
  EXPECT_EQ(controller()->active_variant_for_testing(),
            ContextualTasksOnboardingBubbleView::Variant::kAskGoogle);

  EXPECT_CALL(*mock_tracker(),
              NotifyEvent("contextual_tasks_onboarding_dismissed"))
      .Times(0);

  views::Widget* widget = controller()->bubble_widget_for_testing();
  auto* bubble_view = views::AsViewClass<ContextualTasksOnboardingBubbleView>(
      widget->widget_delegate()->GetContentsView());
  views::test::WidgetDestroyedWaiter close_waiter(widget);
  views::test::ButtonTestApi(bubble_view->accept_button_for_testing())
      .NotifyDefaultMouseClick();
  close_waiter.Wait();

  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                kContextualTasksAskGTooltipDismissedCount),
            1);
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                kContextualTasksSessionCountPostOnboarding),
            5);
  // Session impression cap suppresses showing `kAskGoogle` again this session.
  HideFaviconsElement();
  CreateAndShowFaviconsElement();
  EXPECT_FALSE(controller()->IsBubbleShowing());
}

TEST_F(ContextualTasksOnboardingTooltipControllerTest,
       AcceptButtonDismissesAndUpdatesPrefsAndTracker) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  profile()->GetPrefs()->SetInteger(kContextualTasksSessionCountPostOnboarding,
                                    5);
  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
  CreateAndShowFaviconsElement();
  ASSERT_TRUE(controller()->IsBubbleShowing());
  views::Widget* widget = controller()->bubble_widget_for_testing();
  ASSERT_NE(widget, nullptr);
  auto* bubble_view = views::AsViewClass<ContextualTasksOnboardingBubbleView>(
      widget->widget_delegate()->GetContentsView());
  EXPECT_CALL(*mock_tracker(),
              NotifyEvent("contextual_tasks_onboarding_dismissed"))
      .Times(1);
  views::test::WidgetDestroyedWaiter close_waiter(widget);
  views::test::ButtonTestApi(bubble_view->accept_button_for_testing())
      .NotifyDefaultMouseClick();
  close_waiter.Wait();

  EXPECT_FALSE(controller()->IsBubbleShowing());
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                kContextualTasksOnboardingTooltipDismissedCount),
            1);
  EXPECT_EQ(profile()->GetPrefs()->GetInteger(
                kContextualTasksSessionCountPostOnboarding),
            0);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.OnboardingTooltip.Accepted", true, 1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ContextualTasks.OnboardingTooltip.Accepted"),
            1);
  // Accepted before the impression delay elapsed, so Shown is recorded too.
  histogram_tester.ExpectUniqueSample("ContextualTasks.OnboardingTooltip.Shown",
                                      true, 1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ContextualTasks.OnboardingTooltip.Shown"),
            1);

  // The impression timer was stopped, so Shown isn't recorded again.
  task_environment()->FastForwardBy(base::Milliseconds(
      GetContextualTasksOnboardingTooltipImpressionDelay() + 100));
  histogram_tester.ExpectTotalCount("ContextualTasks.OnboardingTooltip.Shown",
                                    1);
}

TEST_F(ContextualTasksOnboardingTooltipControllerTest,
       AcceptAfterImpressionDelayRecordsShownOnce) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
  CreateAndShowFaviconsElement();
  ASSERT_TRUE(controller()->IsBubbleShowing());
  task_environment()->FastForwardBy(base::Milliseconds(
      GetContextualTasksOnboardingTooltipImpressionDelay() + 100));

  views::Widget* widget = controller()->bubble_widget_for_testing();
  auto* bubble_view = views::AsViewClass<ContextualTasksOnboardingBubbleView>(
      widget->widget_delegate()->GetContentsView());
  views::test::WidgetDestroyedWaiter close_waiter(widget);
  views::test::ButtonTestApi(bubble_view->accept_button_for_testing())
      .NotifyDefaultMouseClick();
  close_waiter.Wait();

  histogram_tester.ExpectUniqueSample("ContextualTasks.OnboardingTooltip.Shown",
                                      true, 1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ContextualTasks.OnboardingTooltip.Shown"),
            1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.OnboardingTooltip.Accepted", true, 1);
}

TEST_F(ContextualTasksOnboardingTooltipControllerTest,
       ShownRecordedOnlyAfterImpressionDelay) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
  CreateAndShowFaviconsElement();
  ASSERT_TRUE(controller()->IsBubbleShowing());
  histogram_tester.ExpectTotalCount("ContextualTasks.OnboardingTooltip.Shown",
                                    0);

  task_environment()->FastForwardBy(base::Milliseconds(
      GetContextualTasksOnboardingTooltipImpressionDelay() + 100));

  histogram_tester.ExpectUniqueSample("ContextualTasks.OnboardingTooltip.Shown",
                                      true, 1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "ContextualTasks.OnboardingTooltip.Shown"),
            1);
}

TEST_F(ContextualTasksOnboardingTooltipControllerTest,
       ShownNotRecordedIfHiddenBeforeImpressionDelay) {
  base::HistogramTester histogram_tester;
  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
  CreateAndShowFaviconsElement();
  ASSERT_TRUE(controller()->IsBubbleShowing());
  HideFaviconsElement();

  task_environment()->FastForwardBy(base::Milliseconds(
      GetContextualTasksOnboardingTooltipImpressionDelay() + 100));

  histogram_tester.ExpectTotalCount("ContextualTasks.OnboardingTooltip.Shown",
                                    0);
}

TEST_F(ContextualTasksOnboardingTooltipControllerTest,
       AskGoogleVariantDoesNotRecordOnboardingMetrics) {
  base::HistogramTester histogram_tester;
  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_OMNIBOX_ACTION);
  CreateAndShowFaviconsElement();
  ASSERT_EQ(controller()->active_variant_for_testing(),
            ContextualTasksOnboardingBubbleView::Variant::kAskGoogle);
  task_environment()->FastForwardBy(base::Milliseconds(
      GetContextualTasksOnboardingTooltipImpressionDelay() + 100));

  views::Widget* widget = controller()->bubble_widget_for_testing();
  auto* bubble_view = views::AsViewClass<ContextualTasksOnboardingBubbleView>(
      widget->widget_delegate()->GetContentsView());
  views::test::WidgetDestroyedWaiter close_waiter(widget);
  views::test::ButtonTestApi(bubble_view->accept_button_for_testing())
      .NotifyDefaultMouseClick();
  close_waiter.Wait();

  histogram_tester.ExpectTotalCount("ContextualTasks.OnboardingTooltip.Shown",
                                    0);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.OnboardingTooltip.Accepted", 0);
}

TEST_F(ContextualTasksOnboardingTooltipControllerTest, DismissedCapRespected) {
  profile()->GetPrefs()->SetInteger(
      kContextualTasksOnboardingTooltipDismissedCount,
      GetContextualTasksOnboardingTooltipDismissedCap());
  profile()->GetPrefs()->SetInteger(
      kContextualTasksAskGTooltipDismissedCount,
      GetContextualTasksAskGTooltipDismissedCap());

  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
  CreateAndShowFaviconsElement();
  EXPECT_FALSE(controller()->IsBubbleShowing());

  HideFaviconsElement();
  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_OMNIBOX_ACTION);
  CreateAndShowFaviconsElement();
  EXPECT_FALSE(controller()->IsBubbleShowing());
}

TEST_F(ContextualTasksOnboardingTooltipControllerTest, BypassDismissedCap) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kContextualTasksBypassDismissedCap);

  profile()->GetPrefs()->SetInteger(
      kContextualTasksOnboardingTooltipDismissedCount,
      GetContextualTasksOnboardingTooltipDismissedCap());

  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
  CreateAndShowFaviconsElement();

  EXPECT_TRUE(controller()->IsBubbleShowing());
  EXPECT_EQ(controller()->active_variant_for_testing(),
            ContextualTasksOnboardingBubbleView::Variant::kOnboarding);
}

TEST_F(ContextualTasksOnboardingTooltipControllerTest, SessionImpressionCap) {
  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
  CreateAndShowFaviconsElement();
  EXPECT_TRUE(controller()->IsBubbleShowing());

  task_environment()->FastForwardBy(base::Milliseconds(
      GetContextualTasksOnboardingTooltipImpressionDelay() + 100));

  HideFaviconsElement();
  EXPECT_FALSE(controller()->IsBubbleShowing());

  CreateAndShowFaviconsElement();
  EXPECT_FALSE(controller()->IsBubbleShowing());
}

TEST_F(ContextualTasksOnboardingTooltipControllerTest,
       RearchitectureDisabledSuppressesBubble) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kContextualTasksRearchitecture,
                             kContextualTasksSidePanelRearchitecture});

  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
  CreateAndShowFaviconsElement();

  EXPECT_FALSE(controller()->IsBubbleShowing());
}

TEST_F(ContextualTasksOnboardingTooltipControllerTest,
       AutomaticActiveTabRequiredForOnboardingOnlyAndHidesWhenCleared) {
  controller()->SetHasAutomaticActiveTabForTesting(false);
  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
  CreateAndShowFaviconsElement();
  EXPECT_FALSE(controller()->IsBubbleShowing());

  // Adding an automatic active tab while favicons are visible shows onboarding.
  controller()->SetHasAutomaticActiveTabForTesting(true);
  EXPECT_TRUE(controller()->IsBubbleShowing());
  EXPECT_EQ(controller()->active_variant_for_testing(),
            ContextualTasksOnboardingBubbleView::Variant::kOnboarding);

  // Removing or submitting the automatic active tab hides onboarding and stops
  // the impression timer even while the favicons element remains visible.
  controller()->SetHasAutomaticActiveTabForTesting(false);
  EXPECT_FALSE(controller()->IsBubbleShowing());
  task_environment()->FastForwardBy(base::Milliseconds(
      GetContextualTasksOnboardingTooltipImpressionDelay() + 100));

  // Because the impression timer was stopped on hide, re-adding an automatic
  // active tab still shows `kOnboarding` under the session impression cap.
  controller()->SetHasAutomaticActiveTabForTesting(true);
  EXPECT_TRUE(controller()->IsBubbleShowing());
  EXPECT_EQ(controller()->active_variant_for_testing(),
            ContextualTasksOnboardingBubbleView::Variant::kOnboarding);
  controller()->SetHasAutomaticActiveTabForTesting(false);

  // `kAskGoogle` does not require a pending delayed automatic tab.
  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_OMNIBOX_ACTION);
  EXPECT_TRUE(controller()->IsBubbleShowing());
  EXPECT_EQ(controller()->active_variant_for_testing(),
            ContextualTasksOnboardingBubbleView::Variant::kAskGoogle);
}

TEST_F(ContextualTasksOnboardingTooltipControllerTest,
       IsAskGoogleEntryPointEligible) {
  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_OMNIBOX_ACTION);
  EXPECT_TRUE(controller()->is_ask_google_entry_point_eligible_for_testing());

  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_OMNIBOX_TAB_SEARCH);
  EXPECT_TRUE(controller()->is_ask_google_entry_point_eligible_for_testing());

  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::
          DESKTOP_CHROME_COBROWSE_OMNIBOX_CONTEXTUAL_SUGGESTION);
  EXPECT_TRUE(controller()->is_ask_google_entry_point_eligible_for_testing());

  controller()->SetEntryPoint(omnibox::ChromeAimEntryPoint::
                                  DESKTOP_CHROME_COBROWSE_OMNIBOX_POPUP_BUTTON);
  EXPECT_TRUE(controller()->is_ask_google_entry_point_eligible_for_testing());

  // Without UpdatedEntryPoints enabled, updated entry points are ineligible.
  controller()->SetEntryPoint(omnibox::ChromeAimEntryPoint::
                                  DESKTOP_CHROME_PAGE_CONTEXT_MENU_ASK_GOOGLE);
  EXPECT_FALSE(controller()->is_ask_google_entry_point_eligible_for_testing());

  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_APP_MENU_ASK_GOOGLE);
  EXPECT_FALSE(controller()->is_ask_google_entry_point_eligible_for_testing());

  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::
          DESKTOP_CHROME_CHROMNIENT_HOMEWORK_ACTION_CHIP);
  EXPECT_FALSE(controller()->is_ask_google_entry_point_eligible_for_testing());
}

TEST_F(ContextualTasksOnboardingTooltipControllerTest,
       IsAskGoogleEntryPointEligible_OmniboxDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(omnibox::kWebUIOmniboxAskGAboutThisPage);

  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_OMNIBOX_ACTION);
  EXPECT_FALSE(controller()->is_ask_google_entry_point_eligible_for_testing());
}

TEST_F(ContextualTasksOnboardingTooltipControllerTest,
       RefreshWebContentsObservations) {
  testing::NiceMock<MockContextualTasksPanelController> mock_panel_controller(
      &mock_browser_window_);
  std::unique_ptr<content::WebContents> active_web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  std::unique_ptr<content::WebContents> toolbar_web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  std::unique_ptr<content::WebContents> second_active_web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

  ON_CALL(mock_panel_controller, GetActiveWebContents())
      .WillByDefault(testing::Return(active_web_contents.get()));
  ON_CALL(mock_panel_controller, GetToolbarWebContents())
      .WillByDefault(testing::Return(toolbar_web_contents.get()));

  EXPECT_EQ(controller()->observed_active_web_contents_for_testing(), nullptr);
  EXPECT_EQ(controller()->observed_toolbar_web_contents_for_testing(), nullptr);

  // SetEntryPoint() triggers RefreshWebContentsObservations().
  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
  EXPECT_EQ(controller()->observed_active_web_contents_for_testing(),
            active_web_contents.get());
  EXPECT_EQ(controller()->observed_toolbar_web_contents_for_testing(),
            toolbar_web_contents.get());

  // If the active WebContents changes, showing the favicons element refreshes
  // observations to the new WebContents.
  ON_CALL(mock_panel_controller, GetActiveWebContents())
      .WillByDefault(testing::Return(second_active_web_contents.get()));
  CreateAndShowFaviconsElement();
  EXPECT_EQ(controller()->observed_active_web_contents_for_testing(),
            second_active_web_contents.get());
  EXPECT_EQ(controller()->observed_toolbar_web_contents_for_testing(),
            toolbar_web_contents.get());

  // CloseBubble() clears the observed WebContents.
  controller()->CloseBubble();
  EXPECT_EQ(controller()->observed_active_web_contents_for_testing(), nullptr);
  EXPECT_EQ(controller()->observed_toolbar_web_contents_for_testing(), nullptr);
}

TEST_F(ContextualTasksOnboardingTooltipControllerTest,
       ActiveWebContentsUserInteractionClosesBubble) {
  testing::NiceMock<MockContextualTasksPanelController> mock_panel_controller(
      &mock_browser_window_);
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  std::unique_ptr<content::WebContents> other_web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  ON_CALL(mock_panel_controller, GetActiveWebContents())
      .WillByDefault(testing::Return(web_contents.get()));

  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
  CreateAndShowFaviconsElement();
  ASSERT_TRUE(controller()->IsBubbleShowing());

  // Interaction on an unobserved WebContents does not close the bubble.
  content::WebContentsTester::For(other_web_contents.get())
      ->TestDidReceiveMouseDownEvent();
  EXPECT_TRUE(controller()->IsBubbleShowing());

  // Interaction on the observed WebContents closes the bubble.
  content::WebContentsTester::For(web_contents.get())
      ->TestDidReceiveMouseDownEvent();

  EXPECT_FALSE(controller()->IsBubbleShowing());
  EXPECT_FALSE(controller()->active_variant_for_testing().has_value());
}

TEST_F(ContextualTasksOnboardingTooltipControllerTest,
       ToolbarWebContentsUserInteractionClosesBubble) {
  testing::NiceMock<MockContextualTasksPanelController> mock_panel_controller(
      &mock_browser_window_);
  std::unique_ptr<content::WebContents> toolbar_web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  ON_CALL(mock_panel_controller, GetToolbarWebContents())
      .WillByDefault(testing::Return(toolbar_web_contents.get()));

  controller()->SetEntryPoint(
      omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON);
  CreateAndShowFaviconsElement();
  ASSERT_TRUE(controller()->IsBubbleShowing());

  content::WebContentsTester::For(toolbar_web_contents.get())
      ->TestDidReceiveMouseDownEvent();

  EXPECT_FALSE(controller()->IsBubbleShowing());
  EXPECT_FALSE(controller()->active_variant_for_testing().has_value());
}

}  // namespace contextual_tasks
