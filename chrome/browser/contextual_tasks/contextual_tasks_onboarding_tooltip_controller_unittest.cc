// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_onboarding_tooltip_controller.h"

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/interaction/element_events.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/window_open_disposition.h"
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
                       WindowOpenDisposition::kNewForegroundTab))
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

}  // namespace contextual_tasks
