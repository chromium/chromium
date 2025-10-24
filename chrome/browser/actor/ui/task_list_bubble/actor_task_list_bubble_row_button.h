// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_ROW_BUTTON_H_
#define CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_ROW_BUTTON_H_

#include "ui/views/controls/button/button.h"

class ActorTaskListBubbleRowButton : public views::Button {
  METADATA_HEADER(ActorTaskListBubbleRowButton, views::Button)

 public:
  explicit ActorTaskListBubbleRowButton();
  ActorTaskListBubbleRowButton(const ActorTaskListBubbleRowButton&) = delete;
  ActorTaskListBubbleRowButton& operator=(const ActorTaskListBubbleRowButton&) =
      delete;
  ~ActorTaskListBubbleRowButton() override;
};

#endif  // CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_ROW_BUTTON_H_
