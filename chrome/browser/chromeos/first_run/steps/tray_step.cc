// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/steps/tray_step.h"

#include "ash/public/cpp/first_run_helper.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/chromeos/first_run/first_run_controller.h"
#include "chrome/browser/chromeos/first_run/step_names.h"
#include "chrome/browser/ui/webui/chromeos/first_run/first_run_actor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chromeos {
namespace first_run {

TrayStep::TrayStep(FirstRunController* controller, FirstRunActor* actor)
    : Step(kTrayStep, controller, actor) {}

void TrayStep::DoShow() {
  // FirstRunController owns this object, so use Unretained.
  gfx::Rect bounds =
      first_run_controller()->first_run_helper()->OpenTrayBubble();
  actor()->AddRectangularHole(bounds.x(), bounds.y(), bounds.width(),
      bounds.height());
  FirstRunActor::StepPosition position;
  position.SetTop(bounds.y());
  ash::ShelfAlignment alignment = first_run_controller()->GetShelfAlignment();
  if ((!base::i18n::IsRTL() && alignment != ash::SHELF_ALIGNMENT_LEFT) ||
      alignment == ash::SHELF_ALIGNMENT_RIGHT) {
    // Compute pixel inset from right side of screen.
    const gfx::Size overlay_size = first_run_controller()->GetOverlaySize();
    position.SetRight(overlay_size.width() - bounds.x());
  } else {
    position.SetLeft(bounds.right());
  }
  actor()->ShowStepPositioned(name(), position);
}

}  // namespace first_run
}  // namespace chromeos
