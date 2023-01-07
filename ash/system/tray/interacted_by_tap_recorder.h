// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_INTERACTED_BY_TAP_RECORDER_H_
#define ASH_SYSTEM_TRAY_INTERACTED_BY_TAP_RECORDER_H_

#include "ui/events/event_handler.h"

namespace views {
class View;
}  // namespace views

namespace ash {

// An event handler that will be installed as system tray view PreTargetHandler
// to record Interaction metrics.
class InteractedByTapRecorder : public ui::EventHandler {
 public:
  explicit InteractedByTapRecorder(views::View* target_view);

  InteractedByTapRecorder(const InteractedByTapRecorder&) = delete;
  InteractedByTapRecorder& operator=(const InteractedByTapRecorder&) = delete;

  ~InteractedByTapRecorder() override = default;

  // Type of interaction. This enum is used to back an UMA histogram and should
  // be treated as append-only.
  enum InteractionType {
    INTERACTION_TYPE_TAP = 0,
    INTERACTION_TYPE_CLICK,
    INTERACTION_TYPE_COUNT
  };

 private:
  // ui::EventHandler:
  void OnEvent(ui::Event* event) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_INTERACTED_BY_TAP_RECORDER_H_
