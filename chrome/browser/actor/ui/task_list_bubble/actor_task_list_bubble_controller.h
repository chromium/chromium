// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_CONTROLLER_H_

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/buildflags.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"
#endif

namespace tabs {
struct ActorTaskListBubbleRowState;
}  // namespace tabs

struct ActorTaskListBubbleRowButtonParams {
  std::u16string title;
  std::u16string subtitle;
  views::Button::PressedCallback on_click_callback;
};

// Controller that handles the visibility and display of the
// ActorTaskListBubble.
class ActorTaskListBubbleController : public views::WidgetObserver {
 public:
  explicit ActorTaskListBubbleController(
      BrowserWindowInterface* browser_window);
  ~ActorTaskListBubbleController() override;

  DECLARE_USER_DATA(ActorTaskListBubbleController);
  static ActorTaskListBubbleController* From(BrowserWindowInterface* window);

#if BUILDFLAG(ENABLE_GLIC)
  void ShowBubble(views::View* anchor_view);
  void OnStateUpdate(actor::TaskId task_id);
#endif

  void OnWidgetDestroyed(views::Widget* widget) override;

  raw_ptr<views::Widget> GetBubbleWidget() { return bubble_widget_; }

 private:
  void GetOnTaskRowClickCallback(actor::TaskId task_id);

  raw_ptr<BrowserWindowInterface> browser_ = nullptr;
  raw_ptr<views::Widget> bubble_widget_ = nullptr;

#if BUILDFLAG(ENABLE_GLIC)
  ActorTaskListBubbleRowButtonParams CreateRowButtonParamsForTaskState(
      tabs::ActorTaskListBubbleRowState task_state);
  void OnStateUpdateImpl(actor::TaskId task_id);

  std::vector<base::CallbackListSubscription>
      bubble_state_change_callback_subscription_;
#endif

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  ui::ScopedUnownedUserData<ActorTaskListBubbleController>
      scoped_unowned_user_data_;

  base::WeakPtrFactory<ActorTaskListBubbleController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_CONTROLLER_H_
