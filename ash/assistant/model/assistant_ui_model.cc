// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_ui_model.h"

#include "ash/assistant/model/assistant_ui_model_observer.h"

namespace ash {

std::ostream& operator<<(std::ostream& os, AssistantVisibility visibility) {
  switch (visibility) {
    case AssistantVisibility::kClosed:
      return os << "Closed";
    case AssistantVisibility::kClosing:
      return os << "Closing";
    case AssistantVisibility::kVisible:
      return os << "Visible";
  }
}

AssistantUiModel::AssistantUiModel() = default;

AssistantUiModel::~AssistantUiModel() = default;

void AssistantUiModel::AddObserver(AssistantUiModelObserver* observer) const {
  observers_.AddObserver(observer);
}

void AssistantUiModel::RemoveObserver(
    AssistantUiModelObserver* observer) const {
  observers_.RemoveObserver(observer);
}

void AssistantUiModel::SetVisible(AssistantEntryPoint entry_point) {
  SetVisibility(AssistantVisibility::kVisible, entry_point,
                /*exit_point=*/std::nullopt);
}

void AssistantUiModel::SetClosing(AssistantExitPoint exit_point) {
  SetVisibility(AssistantVisibility::kClosing,
                /*entry_point=*/std::nullopt, exit_point);
}

void AssistantUiModel::SetClosed(AssistantExitPoint exit_point) {
  SetVisibility(AssistantVisibility::kClosed,
                /*entry_point=*/std::nullopt, exit_point);
}

void AssistantUiModel::SetUsableWorkArea(const gfx::Rect& usable_work_area) {
  if (usable_work_area == usable_work_area_)
    return;

  usable_work_area_ = usable_work_area;
  NotifyUsableWorkAreaChanged();
}

void AssistantUiModel::SetKeyboardTraversalMode(bool keyboard_traversal_mode) {
  if (keyboard_traversal_mode == keyboard_traversal_mode_)
    return;

  keyboard_traversal_mode_ = keyboard_traversal_mode;
  NotifyKeyboardTraversalModeChanged();
}

void AssistantUiModel::SetAppListBubbleWidth(int width) {
  app_list_bubble_width_ = width;
}

void AssistantUiModel::SetVisibility(
    AssistantVisibility visibility,
    std::optional<AssistantEntryPoint> entry_point,
    std::optional<AssistantExitPoint> exit_point) {
  if (visibility == visibility_)
    return;

  const AssistantVisibility old_visibility = visibility_;
  visibility_ = visibility;

  if (visibility == AssistantVisibility::kVisible) {
    // Cache the Assistant entry point used for query count UMA metric.
    DCHECK(entry_point.has_value());
    DCHECK(!exit_point.has_value());
    entry_point_ = entry_point.value();
  } else {
    DCHECK(!entry_point.has_value());
    DCHECK(exit_point.has_value());
  }

  NotifyUiVisibilityChanged(old_visibility, entry_point, exit_point);
}

void AssistantUiModel::NotifyKeyboardTraversalModeChanged() {
  for (AssistantUiModelObserver& observer : observers_)
    observer.OnKeyboardTraversalModeChanged(keyboard_traversal_mode_);
}

void AssistantUiModel::NotifyUiVisibilityChanged(
    AssistantVisibility old_visibility,
    std::optional<AssistantEntryPoint> entry_point,
    std::optional<AssistantExitPoint> exit_point) {
  for (AssistantUiModelObserver& observer : observers_)
    observer.OnUiVisibilityChanged(visibility_, old_visibility, entry_point,
                                   exit_point);
}

void AssistantUiModel::NotifyUsableWorkAreaChanged() {
  for (AssistantUiModelObserver& observer : observers_)
    observer.OnUsableWorkAreaChanged(usable_work_area_);
}

}  // namespace ash
