// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_ROW_BUTTON_H_
#define CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_ROW_BUTTON_H_

#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"

// Button representing a task entry in the ActorTaskListBubble.
class ActorTaskListBubbleRowButton : public RichHoverButton {
  METADATA_HEADER(ActorTaskListBubbleRowButton, RichHoverButton)

 public:
  ActorTaskListBubbleRowButton(views::Button::PressedCallback on_row_clicked,
                               actor::ActorTask::State state,
                               std::u16string title,
                               bool requires_processing,
                               bool has_tab);
  ActorTaskListBubbleRowButton(const ActorTaskListBubbleRowButton&) = delete;
  ActorTaskListBubbleRowButton& operator=(const ActorTaskListBubbleRowButton&) =
      delete;
  ~ActorTaskListBubbleRowButton() override;

  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // HoverButton
  void StateChanged(ButtonState old_state) override;

 private:
  // Update row to reflect an unclickable state.
  void MaybeSetDisabledRowUi();

  // Whether the task in this row has an existing tab or not.
  bool has_tab_;
  // Whether the row has been processed (clicked) or not yet.
  bool requires_processing_;
};

#endif  // CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_ROW_BUTTON_H_
