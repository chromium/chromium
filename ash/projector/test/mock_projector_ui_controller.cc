// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/test/mock_projector_ui_controller.h"

namespace ash {

MockProjectorUiController::MockProjectorUiController(
    ProjectorControllerImpl* projector_controller)
    : ProjectorUiController(projector_controller) {}

MockProjectorUiController::~MockProjectorUiController() = default;

}  // namespace ash
