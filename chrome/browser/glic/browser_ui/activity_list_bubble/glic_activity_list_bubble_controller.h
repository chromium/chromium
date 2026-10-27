// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_ACTIVITY_LIST_BUBBLE_GLIC_ACTIVITY_LIST_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_ACTIVITY_LIST_BUBBLE_GLIC_ACTIVITY_LIST_BUBBLE_CONTROLLER_H_

#include <cstddef>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/browser_ui/activity_list_bubble/glic_activity_row_view_model.h"
#include "chrome/browser/glic/public/service/glic_activity_types.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/buildflags.h"
#include "components/actor/core/task_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class Profile;

namespace glic {

class GlicSplitButtonController;
class GlicActivityListBubbleControllerDelegate;

// Controller that handles the visibility and display of the
// GlicActivityListBubble.
class GlicActivityListBubbleController {
 public:
  // Builds and returns all prioritized row view models for the bubble.
  std::vector<GlicActivityRowViewModel> CreateRowViewModels();

  // Builds and returns the prioritized list of task row view models for the
  // bubble.
  static std::vector<GlicActivityRowViewModel> GetActorTaskRowViewModels(
      Profile* profile,
      const absl::flat_hash_map<actor::TaskId, bool>& task_list,
      base::RepeatingCallback<void(actor::TaskId)> on_task_clicked = {});

  explicit GlicActivityListBubbleController(
      BrowserWindowInterface* browser_window,
      GlicSplitButtonController& split_button_controller);
  ~GlicActivityListBubbleController();

  DECLARE_USER_DATA(GlicActivityListBubbleController);
  static GlicActivityListBubbleController* From(BrowserWindowInterface* window);

  void ShowBubble(bool is_start_notification = false);
  void CloseBubble();
  void OnActivityAlert(ActivityInterruptionLevel level);
  void OnStateUpdate(bool is_start_notification);
  void OnBubbleDestroyed();
  bool IsBubbleShowing() const;

  // Registers a `callback` to be run when the GlicActivityListBubble is shown.
  base::CallbackListSubscription RegisterBubbleShownCallback(
      base::RepeatingClosure callback);

  // Registers a `callback` to be run when the GlicActivityListBubble is
  // destroyed.
  base::CallbackListSubscription RegisterBubbleDestroyedCallback(
      base::RepeatingClosure callback);

  void OnTaskRowClicked(actor::TaskId task_id);
  void OnConversationRowClicked(const std::string& conversation_id);

  base::WeakPtr<GlicActivityListBubbleController> GetWeakPtr();

 private:
  std::vector<GlicActivityRowViewModel> CreateRowViewModelsImpl();
  void ShowBubbleImpl(bool is_start_notification);
  GlicActivityListBubbleControllerDelegate* GetActiveDelegate() const;

  raw_ptr<BrowserWindowInterface> browser_ = nullptr;
  raw_ref<GlicSplitButtonController> split_button_controller_;
  size_t last_row_count_ = 0;
  base::RepeatingClosureList on_bubble_shown_callback_list;
  base::RepeatingClosureList on_bubble_destroyed_callback_list;

  std::vector<base::CallbackListSubscription>
      bubble_state_change_callback_subscription_;

  ui::ScopedUnownedUserData<GlicActivityListBubbleController>
      scoped_unowned_user_data_;

  base::WeakPtrFactory<GlicActivityListBubbleController> weak_ptr_factory_{
      this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_ACTIVITY_LIST_BUBBLE_GLIC_ACTIVITY_LIST_BUBBLE_CONTROLLER_H_
