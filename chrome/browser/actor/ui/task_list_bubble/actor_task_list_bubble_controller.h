// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_CONTROLLER_H_

#include <string_view>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/buildflags.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"


// Controller that handles the visibility and display of the
// ActorTaskListBubble.
class ActorTaskListBubbleController : public views::WidgetObserver {
 public:
  explicit ActorTaskListBubbleController(
      BrowserWindowInterface* browser_window);
  ~ActorTaskListBubbleController() override;

  DECLARE_USER_DATA(ActorTaskListBubbleController);
  static ActorTaskListBubbleController* From(BrowserWindowInterface* window);

  void ShowBubble(views::View* anchor_view);
  void OnStateUpdate();

  void OnWidgetDestroyed(views::Widget* widget) override;

  raw_ptr<views::Widget> GetBubbleWidget() { return bubble_widget_; }

  // Registers a `callback` to be run when the ActorTaskListBubble is shown.
  base::CallbackListSubscription RegisterBubbleShownCallback(
      base::RepeatingClosure callback);

  // Registers a `callback` to be run when the ActorTaskListBubble is destroyed.
  base::CallbackListSubscription RegisterBubbleDestroyedCallback(
      base::RepeatingClosure callback);

 private:
  void OnTaskRowClicked(actor::TaskId task_id);

  raw_ptr<BrowserWindowInterface> browser_ = nullptr;
  raw_ptr<views::Widget> bubble_widget_ = nullptr;
  base::RepeatingClosureList on_bubble_shown_callback_list;
  base::RepeatingClosureList on_bubble_destroyed_callback_list;

  std::vector<base::CallbackListSubscription>
      bubble_state_change_callback_subscription_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  ui::ScopedUnownedUserData<ActorTaskListBubbleController>
      scoped_unowned_user_data_;

  base::WeakPtrFactory<ActorTaskListBubbleController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_CONTROLLER_H_
