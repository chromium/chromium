// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/resources/grit/actor_browser_resources.h"
#include "chrome/browser/actor/ui/actor_ui_interactive_browser_test.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/test/widget_test.h"
#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#endif

class ActorTaskListBubbleInteractiveUiTest
    : public ActorUiInteractiveBrowserTest {
 public:
  ActorTaskListBubbleInteractiveUiTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {
#if BUILDFLAG(ENABLE_GLIC)
            {features::kGlicRollout, {}},
            {features::kGlicFreWarming, {}},
            {features::kGlicActor, {}},
            {features::kGlicActorUi,
             {{features::kGlicActorUiTaskIconName, "true"}}},
            {features::kGlicActorUiNudgeRedesign, {}},
#endif  // BUILDFLAG(ENABLE_GLIC)
        },
        {});
  }

 protected:
#if BUILDFLAG(ENABLE_GLIC)
  glic::GlicTestEnvironment glic_test_environment_;
#endif
  base::test::ScopedFeatureList feature_list_;
};

#if BUILDFLAG(ENABLE_GLIC)
IN_PROC_BROWSER_TEST_F(ActorTaskListBubbleInteractiveUiTest,
                       ShowBubbleWithTask) {
  gfx::ScopedAnimationDurationScaleMode disable_animations(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  const char kFirstTaskItem[] = "FirstTaskItem";
  StartActingOnTab();
  RunTestSequence(
      Do([&]() { PauseTask(); }),
      InAnyContext(WaitForShow(kActorTaskListBubbleView)),
      CheckView(
          kActorTaskListBubbleView,
          [](views::View* view) { return view->children().size() == 1u; }),
      InSameContext(NameDescendantViewByType<RichHoverButton>(
          kActorTaskListBubbleView, kFirstTaskItem, 0)),
      CheckViewProperty(kFirstTaskItem, &RichHoverButton::GetSubtitleText,
                        l10n_util::GetStringUTF16(
                            IDR_ACTOR_TASK_LIST_BUBBLE_CHECK_TASK_SUBTITLE)),
      PressButton(kFirstTaskItem),
      InAnyContext(WaitForHide(kActorTaskListBubbleView)));
}

IN_PROC_BROWSER_TEST_F(ActorTaskListBubbleInteractiveUiTest,
                       ClickingOnTaskInBubbleActuatesTab) {
  gfx::ScopedAnimationDurationScaleMode disable_animations(
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  auto* tab_one = browser()->GetActiveTabInterface();
  StartActingOnTab();

  // Add and activate the non-actuation tab.
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 1,
                                     GURL(chrome::kChromeUINewTabURL),
                                     ui::PAGE_TRANSITION_LINK));
  auto* tab_two = browser()->GetTabStripModel()->GetTabAtIndex(1);
  browser()->GetTabStripModel()->ActivateTabAt(1);
  EXPECT_FALSE(tab_one->IsActivated());
  EXPECT_TRUE(tab_two->IsActivated());

  const char kFirstTaskItem[] = "FirstTaskItem";
  RunTestSequence(
      Do([&]() { PauseTask(); }),
      InAnyContext(WaitForShow(kActorTaskListBubbleView)),
      CheckView(
          kActorTaskListBubbleView,
          [](views::View* view) { return view->children().size() == 1u; }),
      InSameContext(NameDescendantViewByType<RichHoverButton>(
          kActorTaskListBubbleView, kFirstTaskItem, 0)),
      CheckViewProperty(kFirstTaskItem, &RichHoverButton::GetSubtitleText,
                        l10n_util::GetStringUTF16(
                            IDR_ACTOR_TASK_LIST_BUBBLE_CHECK_TASK_SUBTITLE)),
      PressButton(kFirstTaskItem),
      InAnyContext(WaitForHide(kActorTaskListBubbleView)));

  EXPECT_TRUE(tab_one->IsActivated());
  EXPECT_FALSE(tab_two->IsActivated());
}
#endif
