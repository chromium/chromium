// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_OBSERVER_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_OBSERVER_H_

#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

enum class AssistantSource;
enum class AssistantUiMode;
enum class AssistantVisibility;

// An observer which receives notification of changes to the Assistant UI model.
class AssistantUiModelObserver {
 public:
  // Invoked when the UI mode is changed.
  virtual void OnUiModeChanged(AssistantUiMode ui_mode) {}

  // Invoked when the UI visibility is changed from |old_visibility| to
  // |new_visibility|. The |source| of the visibility change event is provided
  // for interested observers.
  virtual void OnUiVisibilityChanged(AssistantVisibility new_visibility,
                                     AssistantVisibility old_visibility,
                                     AssistantSource source) {}

  // Invoked when the usable display work area is changed. Observers should
  // respond to this event by ensuring they are sized/positioned within the
  // |usable_work_area|.
  virtual void OnUsableWorkAreaChanged(const gfx::Rect& usable_work_area) {}

 protected:
  AssistantUiModelObserver() = default;
  virtual ~AssistantUiModelObserver() = default;

  DISALLOW_COPY_AND_ASSIGN(AssistantUiModelObserver);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_OBSERVER_H_
