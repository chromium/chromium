// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/activity_list_bubble/glic_activity_list_bubble.h"

#include <string_view>

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/browser_ui/activity_list_bubble/glic_activity_list_bubble_controller.h"
#include "chrome/browser/glic/browser_ui/activity_list_bubble/glic_activity_list_bubble_row_button.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/service/glic_activity_manager.h"
#include "chrome/browser/glic/public/service/glic_activity_types.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

namespace glic {
namespace {

class GlicActivityHubBubbleBrowserTest : public GlicBrowserTest {
 public:
  GlicActivityHubBubbleBrowserTest() {
    SetGlicPagePath("/glic/test_client/index.html");
    feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kGlicRollout, {}},
            {features::kGlicActor, {}},
            {features::kGlicActorPolicyControlExemption, {}},
            {features::kGlicActorUi,
             {{features::kGlicActorUiTaskIconName, "true"}}},
            {features::kGlicActivityHub, {}},
        },
        {});
  }

 protected:
  GlicActivityManager& activity_manager() {
    return service()->activity_manager();
  }

  GlicActivityListBubbleController* bubble_controller(
      BrowserWindowInterface* window) {
    auto* controller = GlicActivityListBubbleController::From(window);
    CHECK(controller);
    return controller;
  }

  // Showing the secondary chevron starts a SlideAnimation from width_factor 0
  // to 1. Wait until layout assigns non-zero bounds so FlexLayout does not hide
  // a 0-width icon.
  [[nodiscard]] TestResult<views::Button*> WaitForSecondaryButton(
      BrowserWindowInterface* window) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(window);
    CHECK(browser_view);
    views::Button* button = nullptr;
    bool found = RunUntil(
        [&]() {
          views::test::RunScheduledLayout(browser_view);
          views::View* icon_view =
              views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                  kGlicActorTaskIconElementId,
                  browser_view->GetElementContext());
          if (icon_view && icon_view->GetVisible() && icon_view->width() > 0) {
            button = views::Button::AsButton(icon_view);
            return button != nullptr;
          }
          return false;
        },
        "Secondary split button did not become visible with non-zero width");
    if (!found) {
      return base::unexpected(
          "Secondary split button did not become visible with non-zero width");
    }
    return button;
  }

  views::View* GetBubbleContentsView(BrowserWindowInterface* window) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(window);
    CHECK(browser_view);
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
        kActorTaskListBubbleView, browser_view->GetElementContext(),
        /*require_visible=*/true);
  }

  void ClickButton(views::Button* button) {
    CHECK(button);
    views::test::ButtonTestApi(button).NotifyClick(
        ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON));
  }

  [[nodiscard]] testing::AssertionResult RegisterConversationInClient(
      GlicInstanceImpl* instance,
      std::string_view conversation_id,
      std::string_view title) {
    return content::ExecJs(instance->host().web_client_contents(),
                           content::JsReplace(
                               R"js(
                                 window.client.browser.registerConversation({
                                   conversationId: $1,
                                   conversationTitle: $2,
                                 })
                               )js",
                               conversation_id, title));
  }

  [[nodiscard]] testing::AssertionResult NotifyUserInputSubmittedInClient(
      GlicInstanceImpl* instance) {
    return content::ExecJs(instance->host().web_client_contents(),
                           R"js(
          window.client.browser.getMetrics().onUserInputSubmitted(0);
        )js");
  }

  [[nodiscard]] testing::AssertionResult NotifyResponseStartedInClient(
      GlicInstanceImpl* instance) {
    return content::ExecJs(instance->host().web_client_contents(),
                           R"js(
          window.client.browser.getMetrics().onResponseStarted();
        )js");
  }

  [[nodiscard]] testing::AssertionResult NotifyResponseStoppedInClient(
      GlicInstanceImpl* instance) {
    return content::ExecJs(instance->host().web_client_contents(),
                           R"js(
          window.client.browser.getMetrics().onResponseStopped();
        )js");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    GlicActivityHubBubbleBrowserTest,
    SplitChevronOpensBubbleAndClickingRowFocusesInstanceAndTab) {
  gfx::ScopedAnimationDurationScaleMode disable_animations(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance1, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance1));
  ASSERT_TRUE(
      RegisterConversationInClient(instance1, "conv_1", "Trip Planner"));
  ASSERT_TRUE(NotifyUserInputSubmittedInClient(instance1));

  // Switch to tab2, register an idle conversation, and close tab2's panel.
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance2, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance2));
  ASSERT_TRUE(
      RegisterConversationInClient(instance2, "conv_2", "Packing List"));
  ASSERT_TRUE(NotifyUserInputSubmittedInClient(instance2));
  ToggleGlicForActiveTab();
  ASSERT_OK(WaitForGlicClose(instance2));

  // Complete a background turn on conv_1 while tab2 is active so conv_1 is
  // unseen and prioritized ahead of idle conv_2.
  ASSERT_TRUE(NotifyResponseStartedInClient(instance1));
  ASSERT_TRUE(NotifyResponseStoppedInClient(instance1));
  ASSERT_OK(RunUntilEqual(
      [&]() { return activity_manager().GetNumUnseenActivities(); }, 1u));

  const GlicActivityKey conv1_key{GlicActivityType::kConversationResponse,
                                  "conv_1"};
  const GlicActivityKey conv2_key{GlicActivityType::kConversationResponse,
                                  "conv_2"};
  auto conv1_it = activity_manager().activities().find(conv1_key);
  ASSERT_NE(conv1_it, activity_manager().activities().end());
  EXPECT_FALSE(conv1_it->second.has_been_seen);
  auto conv2_it = activity_manager().activities().find(conv2_key);
  ASSERT_NE(conv2_it, activity_manager().activities().end());
  EXPECT_TRUE(conv2_it->second.has_been_seen);

  ASSERT_OK_AND_ASSIGN(views::Button * secondary_button,
                       WaitForSecondaryButton(GetBrowser()));
  ClickButton(secondary_button);
  ASSERT_TRUE(bubble_controller(GetBrowser())->IsBubbleShowing());

  views::View* bubble_view = GetBubbleContentsView(GetBrowser());
  ASSERT_NE(bubble_view, nullptr);
  ASSERT_EQ(bubble_view->children().size(), 2u);

  auto* row_0 = views::AsViewClass<GlicActivityListBubbleRowButton>(
      bubble_view->children()[0]);
  auto* row_1 = views::AsViewClass<GlicActivityListBubbleRowButton>(
      bubble_view->children()[1]);
  ASSERT_NE(row_0, nullptr);
  ASSERT_NE(row_1, nullptr);
  EXPECT_EQ(row_0->GetTitleText(), u"Trip Planner");
  EXPECT_EQ(row_0->GetSubtitleText(),
            l10n_util::GetStringUTF16(IDS_GLIC_ACTIVITY_RESPONSE_READY));
  EXPECT_EQ(row_1->GetTitleText(), u"Packing List");
  EXPECT_EQ(row_1->GetSubtitleText(), u"");

  views::test::WidgetDestroyedWaiter bubble_destroyed(bubble_view->GetWidget());
  ClickButton(row_0);
  bubble_destroyed.Wait();
  EXPECT_FALSE(bubble_controller(GetBrowser())->IsBubbleShowing());

  conv1_it = activity_manager().activities().find(conv1_key);
  ASSERT_NE(conv1_it, activity_manager().activities().end());
  EXPECT_TRUE(conv1_it->second.has_been_seen);
  EXPECT_EQ(activity_manager().GetNumUnseenActivities(), 0u);

  EXPECT_EQ(GetTabListInterface()->GetActiveTab(), tab1);
  EXPECT_TRUE(tab1->IsActivated());
  EXPECT_FALSE(tab2->IsActivated());
  ASSERT_OK(WaitForGlicOpen(instance1));
}

IN_PROC_BROWSER_TEST_F(
    GlicActivityHubBubbleBrowserTest,
    ClickingConversationRowActivatesWindowAndTabAcrossBrowsers) {
  gfx::ScopedAnimationDurationScaleMode disable_animations(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance1, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance1));
  ASSERT_TRUE(RegisterConversationInClient(instance1, "conv_window_1",
                                           "Window 1 Chat"));
  ASSERT_TRUE(NotifyUserInputSubmittedInClient(instance1));
  ToggleGlicForActiveTab();
  ASSERT_OK(WaitForGlicClose(instance1));

  const GlicActivityKey key{GlicActivityType::kConversationResponse,
                            "conv_window_1"};
  ASSERT_OK(RunUntilEqual(
      [&]() { return activity_manager().activities().contains(key); }, true));

  BrowserWindowInterface* browser_2 = CreateAdditionalBrowserWindow();
  ASSERT_OK_AND_ASSIGN(views::Button * secondary_button_2,
                       WaitForSecondaryButton(browser_2));
  ClickButton(secondary_button_2);
  ASSERT_TRUE(bubble_controller(browser_2)->IsBubbleShowing());

  views::View* bubble_view = GetBubbleContentsView(browser_2);
  ASSERT_NE(bubble_view, nullptr);
  ASSERT_EQ(bubble_view->children().size(), 1u);

  auto* row_0 = views::AsViewClass<GlicActivityListBubbleRowButton>(
      bubble_view->children()[0]);
  ASSERT_NE(row_0, nullptr);
  EXPECT_EQ(row_0->GetTitleText(), u"Window 1 Chat");

  views::test::WidgetDestroyedWaiter bubble_destroyed(bubble_view->GetWidget());
  ClickButton(row_0);
  bubble_destroyed.Wait();
  EXPECT_FALSE(bubble_controller(browser_2)->IsBubbleShowing());

  ASSERT_OK(WaitForWindowActive(GetBrowser()));
  EXPECT_TRUE(tab1->IsActivated());
  ASSERT_OK(WaitForGlicOpen(instance1));
}

IN_PROC_BROWSER_TEST_F(GlicActivityHubBubbleBrowserTest,
                       SplitChevronTogglesBubbleOnReclick) {
  gfx::ScopedAnimationDurationScaleMode disable_animations(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  ASSERT_TRUE(
      RegisterConversationInClient(instance, "conv_toggle", "Toggle Chat"));
  ASSERT_TRUE(NotifyUserInputSubmittedInClient(instance));
  ToggleGlicForActiveTab();
  ASSERT_OK(WaitForGlicClose(instance));

  const GlicActivityKey key{GlicActivityType::kConversationResponse,
                            "conv_toggle"};
  ASSERT_OK(RunUntilEqual(
      [&]() { return activity_manager().activities().contains(key); }, true));

  ASSERT_OK_AND_ASSIGN(views::Button * secondary_button,
                       WaitForSecondaryButton(GetBrowser()));
  ClickButton(secondary_button);
  ASSERT_TRUE(bubble_controller(GetBrowser())->IsBubbleShowing());

  views::View* bubble_view = GetBubbleContentsView(GetBrowser());
  ASSERT_NE(bubble_view, nullptr);
  views::test::WidgetDestroyedWaiter bubble_destroyed(bubble_view->GetWidget());

  // Re-clicking the split chevron while the bubble is open closes it cleanly
  // without re-opening.
  ClickButton(secondary_button);
  bubble_destroyed.Wait();
  EXPECT_FALSE(bubble_controller(GetBrowser())->IsBubbleShowing());
}

}  // namespace
}  // namespace glic
