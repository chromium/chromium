// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_ui_model.h"

#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/cpp/app_list/app_list_features.h"

namespace ash {

AssistantUiModel::AssistantUiModel()
    : ui_mode_(app_list_features::IsAssistantLauncherUIEnabled()
                   ? AssistantUiMode::kLauncherEmbeddedUi
                   : AssistantUiMode::kMainUi) {}

AssistantUiModel::~AssistantUiModel() = default;

void AssistantUiModel::AddObserver(AssistantUiModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AssistantUiModel::RemoveObserver(AssistantUiModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantUiModel::SetUiMode(AssistantUiMode ui_mode,
                                 bool due_to_interaction) {
  if (ui_mode == ui_mode_)
    return;

  ui_mode_ = ui_mode;
  NotifyUiModeChanged(due_to_interaction);
}

void AssistantUiModel::SetVisible(AssistantEntryPoint entry_point) {
  SetVisibility(AssistantVisibility::kVisible, entry_point,
                /*exit_point=*/base::nullopt);
}

void AssistantUiModel::SetHidden(AssistantExitPoint exit_point) {
  SetVisibility(AssistantVisibility::kHidden,
                /*entry_point=*/base::nullopt, exit_point);
}

void AssistantUiModel::SetClosed(AssistantExitPoint exit_point) {
  SetVisibility(AssistantVisibility::kClosed,
                /*entry_point=*/base::nullopt, exit_point);
}

void AssistantUiModel::SetUsableWorkArea(const gfx::Rect& usable_work_area) {
  if (usable_work_area == usable_work_area_)
    return;

  usable_work_area_ = usable_work_area;
  NotifyUsableWorkAreaChanged();
}

void AssistantUiModel::SetVisibility(
    AssistantVisibility visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
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

void AssistantUiModel::NotifyUiModeChanged(bool due_to_interaction) {
  for (AssistantUiModelObserver& observer : observers_)
    observer.OnUiModeChanged(ui_mode_, due_to_interaction);
}

void AssistantUiModel::NotifyUiVisibilityChanged(
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  for (AssistantUiModelObserver& observer : observers_)
    observer.OnUiVisibilityChanged(visibility_, old_visibility, entry_point,
                                   exit_point);
}

void AssistantUiModel::NotifyUsableWorkAreaChanged() {
  for (AssistantUiModelObserver& observer : observers_)
    observer.OnUsableWorkAreaChanged(usable_work_area_);
}

}  // namespace ash
