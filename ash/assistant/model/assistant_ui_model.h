// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_H_

#include <optional>
#include <ostream>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "base/component_export.h"
#include "base/observer_list.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class AssistantUiModelObserver;

// Enumeration of Assistant visibility states.
enum class AssistantVisibility {
  kClosed,   // Assistant UI is hidden and the previous session has finished.
  kClosing,  // Assistant UI is transitioning from `kVisible` to `kClosed`.
  kVisible,  // Assistant UI is visible and a session is in progress.
};

COMPONENT_EXPORT(ASSISTANT_MODEL)
std::ostream& operator<<(std::ostream& os, AssistantVisibility visibility);

// Enumeration of Assistant button ID. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
// Only append to this enum is allowed if more buttons will be added.
enum class AssistantButtonId {
  kBackDeprecated = 1,
  kCloseDeprecated = 2,
  kMinimizeDeprecated = 3,
  kKeyboardInputToggle = 4,
  kVoiceInputToggle = 5,
  kSettingsDeprecated = 6,
  kBackInLauncherDeprecated = 7,
  kMaxValue = kBackInLauncherDeprecated
};

// Models the Assistant UI.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantUiModel {
 public:
  using AssistantEntryPoint = assistant::AssistantEntryPoint;
  using AssistantExitPoint = assistant::AssistantExitPoint;

  AssistantUiModel();

  AssistantUiModel(const AssistantUiModel&) = delete;
  AssistantUiModel& operator=(const AssistantUiModel&) = delete;

  ~AssistantUiModel();

  // Adds/removes the specified |observer|.
  void AddObserver(AssistantUiModelObserver* observer) const;
  void RemoveObserver(AssistantUiModelObserver* observer) const;

  // Sets the UI visibility.
  void SetVisible(AssistantEntryPoint entry_point);
  void SetClosing(AssistantExitPoint exit_point);
  void SetClosed(AssistantExitPoint exit_point);

  AssistantVisibility visibility() const { return visibility_; }

  // Sets the current usable work area.
  void SetUsableWorkArea(const gfx::Rect& usable_work_area);

  // Returns the current usable work area.
  const gfx::Rect& usable_work_area() const { return usable_work_area_; }

  // Returns the UI entry point. Only valid while UI is visible.
  AssistantEntryPoint entry_point() const { return entry_point_; }

  // Sets the current keyboard traversal mode.
  void SetKeyboardTraversalMode(bool keyboard_traversal_mode);

  // Returns the current keyboard traversal mode.
  bool keyboard_traversal_mode() const { return keyboard_traversal_mode_; }

  int AppListBubbleWidth() const { return app_list_bubble_width_; }
  void SetAppListBubbleWidth(int width);

 private:
  void SetVisibility(AssistantVisibility visibility,
                     std::optional<AssistantEntryPoint> entry_point,
                     std::optional<AssistantExitPoint> exit_point);

  void NotifyKeyboardTraversalModeChanged();
  void NotifyUiModeChanged(bool due_to_interaction);
  void NotifyUiVisibilityChanged(AssistantVisibility old_visibility,
                                 std::optional<AssistantEntryPoint> entry_point,
                                 std::optional<AssistantExitPoint> exit_point);
  void NotifyUsableWorkAreaChanged();

  AssistantVisibility visibility_ = AssistantVisibility::kClosed;
  AssistantEntryPoint entry_point_ = AssistantEntryPoint::kUnspecified;
  int app_list_bubble_width_ = kPreferredWidthDip;

  mutable base::ObserverList<AssistantUiModelObserver> observers_;

  // Usable work area for Assistant. Value is only meaningful when Assistant
  // UI exists.
  gfx::Rect usable_work_area_;

  // Whether or not keyboard traversal is currently enabled.
  // Used for updating the Assistant UI when it exists.
  bool keyboard_traversal_mode_ = false;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_H_
