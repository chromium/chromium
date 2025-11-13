// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_H_
#define CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_H_

#include <memory>
#include <vector>

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"

namespace views {
class View;
}  // namespace views

DECLARE_ELEMENT_IDENTIFIER_VALUE(kActorTaskListBubbleView);

// Bubble that displays notifications about the user's ongoing tasks.
class ActorTaskListBubble {
 public:
  static views::Widget* ShowBubble(
      views::View* anchor_view,
      std::vector<ActorTaskListBubbleRowButtonParams> params);

 private:
  static std::unique_ptr<views::View> CreateContentsView(
      std::vector<ActorTaskListBubbleRowButtonParams> param_list);
};

#endif  // CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_H_
