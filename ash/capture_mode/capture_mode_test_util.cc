// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_test_util.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view.h"

namespace ash {

CaptureModeController* StartCaptureSession(CaptureModeSource source,
                                           CaptureModeType type) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(source);
  controller->SetType(type);
  controller->Start(CaptureModeEntryType::kQuickSettings);
  DCHECK(controller->IsActive());
  return controller;
}

void ClickOnView(const views::View* view,
                 ui::test::EventGenerator* event_generator) {
  DCHECK(view);
  DCHECK(event_generator);

  const gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(view_center);
  event_generator->ClickLeftButton();
}

}  // namespace ash