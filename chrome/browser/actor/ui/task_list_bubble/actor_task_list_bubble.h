// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_H_
#define CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_H_

#include <memory>
#include <vector>

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace views {
class View;
}  // namespace views

DECLARE_ELEMENT_IDENTIFIER_VALUE(kActorTaskListBubbleView);

class Profile;

// Bubble that displays notifications about the user's ongoing tasks.
class ActorTaskListBubble {
 public:
  static views::Widget* ShowBubble(
      Profile* profile,
      views::View* anchor_view,
      const absl::flat_hash_map<actor::TaskId, bool>& task_list,
      base::RepeatingCallback<void(actor::TaskId)> on_row_clicked);

 private:
  static std::unique_ptr<views::View> CreateContentsView(
      Profile* profile,
      const absl::flat_hash_map<actor::TaskId, bool>& task_list,
      base::RepeatingCallback<void(actor::TaskId)> on_row_clicked);
};

#endif  // CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_H_
