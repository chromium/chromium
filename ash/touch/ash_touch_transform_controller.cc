// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/ash_touch_transform_controller.h"

#include "ash/shell.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/touch_transform_setter.h"

namespace ash {

AshTouchTransformController::AshTouchTransformController(
    display::DisplayManager* display_manager,
    std::unique_ptr<display::TouchTransformSetter> setter)
    : TouchTransformController(display_manager, std::move(setter)) {
  Shell::Get()->display_manager()->AddDisplayManagerObserver(this);
}

AshTouchTransformController::~AshTouchTransformController() {
  Shell::Get()->display_manager()->RemoveDisplayManagerObserver(this);
}

void AshTouchTransformController::OnDisplaysInitialized() {
  UpdateTouchTransforms();
}

void AshTouchTransformController::OnDidApplyDisplayChanges() {
  UpdateTouchTransforms();
}

}  // namespace ash
