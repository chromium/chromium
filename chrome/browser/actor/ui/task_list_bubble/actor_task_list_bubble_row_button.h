// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_ROW_BUTTON_H_
#define CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_ROW_BUTTON_H_

#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

// Button representing a task entry in the ActorTaskListBubble.
class ActorTaskListBubbleRowButton : public views::Button {
  METADATA_HEADER(ActorTaskListBubbleRowButton, views::Button)

 public:
  ActorTaskListBubbleRowButton(views::Button::PressedCallback on_row_clicked,
                               actor::ActorTask::State state,
                               std::u16string title_text,
                               bool requires_processing,
                               bool has_tab);
  ActorTaskListBubbleRowButton(const ActorTaskListBubbleRowButton&) = delete;
  ActorTaskListBubbleRowButton& operator=(const ActorTaskListBubbleRowButton&) =
      delete;
  ~ActorTaskListBubbleRowButton() override;

  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  std::u16string_view GetTitleText() const;
  std::u16string_view GetSubtitleText() const;
  views::ImageButton* GetRedirectIconForTesting() { return redirect_icon_; }

 private:
  // Update row to reflect an unclickable state.
  void MaybeSetDisabledRowUi();

  // Forwards clicks on the redirect icon to the row button.
  void OnRedirectIconPressed(const ui::Event& event);

  // Update the accessible name for the button based on the title and subtitle
  // text.
  void UpdateAccessibleName();

  // Whether the task in this row has an existing tab or not.
  bool has_tab_;
  // Whether the row has been processed (clicked) or not yet.
  bool requires_processing_;

  raw_ptr<views::ImageView> row_icon_ = nullptr;
  raw_ptr<views::ImageButton> redirect_icon_ = nullptr;
  raw_ptr<views::Label> title_;  // Never null.
  raw_ptr<views::Label> subtitle_ = nullptr;
};

#endif  // CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_ROW_BUTTON_H_
