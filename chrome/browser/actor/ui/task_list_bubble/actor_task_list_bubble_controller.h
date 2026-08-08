// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_CONTROLLER_H_

#include <string_view>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/buildflags.h"
#include "components/actor/core/task_id.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace glic {
class GlicSplitButtonController;
}
class ActorTaskListBubbleControllerDelegate;

// Controller that handles the visibility and display of the
// ActorTaskListBubble.
class ActorTaskListBubbleController {
 public:
  explicit ActorTaskListBubbleController(
      BrowserWindowInterface* browser_window,
      glic::GlicSplitButtonController& split_button_controller);
  ~ActorTaskListBubbleController();

  DECLARE_USER_DATA(ActorTaskListBubbleController);
  static ActorTaskListBubbleController* From(BrowserWindowInterface* window);

  void ShowBubble(bool is_start_notification = false);
  void CloseBubble();
  void OnStateUpdate(bool is_start_notification);
  void OnBubbleDestroyed();
  bool IsBubbleShowing() const;

  // Registers a `callback` to be run when the ActorTaskListBubble is shown.
  base::CallbackListSubscription RegisterBubbleShownCallback(
      base::RepeatingClosure callback);

  // Registers a `callback` to be run when the ActorTaskListBubble is destroyed.
  base::CallbackListSubscription RegisterBubbleDestroyedCallback(
      base::RepeatingClosure callback);

  void OnTaskRowClicked(actor::TaskId task_id);

 private:
  void ShowBubbleImpl(bool is_start_notification);
  ActorTaskListBubbleControllerDelegate* GetActiveDelegate() const;

  raw_ptr<BrowserWindowInterface> browser_ = nullptr;
  raw_ref<glic::GlicSplitButtonController> split_button_controller_;
  base::RepeatingClosureList on_bubble_shown_callback_list;
  base::RepeatingClosureList on_bubble_destroyed_callback_list;

  std::vector<base::CallbackListSubscription>
      bubble_state_change_callback_subscription_;

  ui::ScopedUnownedUserData<ActorTaskListBubbleController>
      scoped_unowned_user_data_;

  base::WeakPtrFactory<ActorTaskListBubbleController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_CONTROLLER_H_
