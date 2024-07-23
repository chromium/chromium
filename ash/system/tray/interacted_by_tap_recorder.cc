// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/interacted_by_tap_recorder.h"

#include "base/metrics/histogram_macros.h"
#include "ui/events/event.h"
#include "ui/views/view.h"

namespace ash {

InteractedByTapRecorder::InteractedByTapRecorder(views::View* target_view) {
  target_view->AddPreTargetHandler(this);
}

void InteractedByTapRecorder::OnEvent(ui::Event* event) {
  if (event->type() == ui::EventType::kGestureTap) {
    UMA_HISTOGRAM_ENUMERATION("ChromeOS.SystemTray.Interaction",
                              INTERACTION_TYPE_TAP, INTERACTION_TYPE_COUNT);
  } else if (event->type() == ui::EventType::kMousePressed) {
    UMA_HISTOGRAM_ENUMERATION("ChromeOS.SystemTray.Interaction",
                              INTERACTION_TYPE_CLICK, INTERACTION_TYPE_COUNT);
  }
}

}  // namespace ash
