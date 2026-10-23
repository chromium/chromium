// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_onboarding_tooltip_controller.h"

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_onboarding_bubble_view.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/contextual_tasks/public/features.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/interaction/element_events.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/md_text_button.h"
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
                              kContextualTasksShowOnboardingTooltip},
        /*disabled_features=*/{});
    TestingProfile::Builder builder;
    builder.AddTestingFactory(feature_engagement::TrackerFactory::GetInstance(),
                              base::BindRepeating(&BuildMockTracker));
    profile_ = builder.Build();
    ON_CALL(mock_browser_window_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));
    controller_ = std::make_unique<ContextualTasksOnboardingTooltipController>(
        &mock_browser_window_, profile_.get());
    controller_->SetContextOverrideForTesting(kTestContext);
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
       AcceptButtonDismissesAndNotifiesTracker) {
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
}

}  // namespace contextual_tasks
