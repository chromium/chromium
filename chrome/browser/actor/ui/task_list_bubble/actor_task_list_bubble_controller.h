// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_CONTROLLER_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/glic_enums.mojom-forward.h"
#include "components/actor/core/task_id.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class Profile;

namespace glic {
class GlicSplitButtonController;
}
class ActorTaskListBubbleControllerDelegate;

namespace actor::ui {

struct ActorTaskRowData {
  actor::TaskId task_id;
  std::string title;
  actor::ActorTask::State state;
  bool requires_processing = false;
  bool has_tab = false;
  int tab_id = -1;
  glic::mojom::FeatureMode feature_mode;
  std::optional<actor::ActorTask::InterruptReason> interrupt_reason;
};

}  // namespace actor::ui

// Controller that handles the visibility and display of the
// ActorTaskListBubble.
class ActorTaskListBubbleController {
 public:
  // Builds and returns the prioritized list of task rows for the bubble.
  static std::vector<actor::ui::ActorTaskRowData> GetActorTaskRowsForBubble(
      Profile* profile,
      const absl::flat_hash_map<actor::TaskId, bool>& task_list);
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
