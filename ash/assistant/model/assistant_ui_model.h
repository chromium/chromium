// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class AssistantUiModelObserver;

// Enumeration of Assistant entry/exit points, also recorded in histograms.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Only append to this enum is allowed
// if the possible source grows.
enum class AssistantSource {
  kUnspecified = 0,
  kDeepLink = 1,
  kHotkey = 2,
  kHotword = 3,
  kLauncherSearchBox = 4,
  kLongPressLauncher = 5,
  kSetup = 6,
  kStylus = 7,
  // Special enumerator value used by histogram macros.
  kMaxValue = kStylus
};

// Enumeration of Assistant UI modes.
enum class AssistantUiMode {
  kMainUi,
  kMiniUi,
  kWebUi,
};

// Enumeration of Assistant visibility states.
enum class AssistantVisibility {
  kClosed,   // Assistant UI is hidden and the previous session has finished.
  kHidden,   // Assistant UI is hidden and the previous session is paused.
  kVisible,  // Assistant UI is visible and a session is in progress.
};

// Models the Assistant UI.
class AssistantUiModel {
 public:
  AssistantUiModel();
  ~AssistantUiModel();

  // Adds/removes the specified |observer|.
  void AddObserver(AssistantUiModelObserver* observer);
  void RemoveObserver(AssistantUiModelObserver* observer);

  // Sets the UI mode.
  void SetUiMode(AssistantUiMode ui_mode);

  // Returns the UI mode.
  AssistantUiMode ui_mode() const { return ui_mode_; }

  // Sets the UI visibility.
  void SetVisibility(AssistantVisibility visibility, AssistantSource source);

  AssistantVisibility visibility() const { return visibility_; }

  // Sets the current usable work area.
  void SetUsableWorkArea(const gfx::Rect& usable_work_area);

  // Returns the current usable work area.
  const gfx::Rect& usable_work_area() const { return usable_work_area_; }

  // Returns the UI entry point. Only valid while UI is visible.
  AssistantSource entry_point() const { return entry_point_; }

 private:
  void NotifyUiModeChanged();
  void NotifyUiVisibilityChanged(AssistantVisibility old_visibility,
                                 AssistantSource source);
  void NotifyUsableWorkAreaChanged();

  AssistantUiMode ui_mode_ = AssistantUiMode::kMainUi;

  AssistantVisibility visibility_ = AssistantVisibility::kClosed;

  AssistantSource entry_point_ = AssistantSource::kUnspecified;

  base::ObserverList<AssistantUiModelObserver>::Unchecked observers_;

  // Usable work area for Assistant. Value is only meaningful when Assistant
  // UI exists.
  gfx::Rect usable_work_area_;

  DISALLOW_COPY_AND_ASSIGN(AssistantUiModel);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_H_
