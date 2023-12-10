// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_OBSERVER_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_OBSERVER_H_

#include <optional>

#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace assistant {
enum class AssistantEntryPoint;
enum class AssistantExitPoint;
}  // namespace assistant

enum class AssistantVisibility;

// A checked observer which receives notification of changes to the Assistant UI
// model.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantUiModelObserver
    : public base::CheckedObserver {
 public:
  using AssistantEntryPoint = assistant::AssistantEntryPoint;
  using AssistantExitPoint = assistant::AssistantExitPoint;

  AssistantUiModelObserver(const AssistantUiModelObserver&) = delete;
  AssistantUiModelObserver& operator=(const AssistantUiModelObserver&) = delete;

  // Invoked when keyboard traversal mode is changed (enabled/disabled).
  virtual void OnKeyboardTraversalModeChanged(bool keyboard_traversal_mode) {}

  // Invoked when the UI visibility is changed from |old_visibility| to
  // |new_visibility|. The |source| of the visibility change event is provided
  // for interested observers.
  virtual void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      std::optional<AssistantEntryPoint> entry_point,
      std::optional<AssistantExitPoint> exit_point) {}

  // Invoked when the usable display work area is changed. Observers should
  // respond to this event by ensuring they are sized/positioned within the
  // |usable_work_area|.
  virtual void OnUsableWorkAreaChanged(const gfx::Rect& usable_work_area) {}

 protected:
  AssistantUiModelObserver() = default;
  ~AssistantUiModelObserver() override = default;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_OBSERVER_H_
