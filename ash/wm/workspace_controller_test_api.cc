// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace_controller_test_api.h"

#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "ui/aura/window.h"

namespace ash {

WorkspaceControllerTestApi::WorkspaceControllerTestApi(
    WorkspaceController* controller)
    : controller_(controller) {}

WorkspaceControllerTestApi::~WorkspaceControllerTestApi() = default;

WorkspaceEventHandler* WorkspaceControllerTestApi::GetEventHandler() {
  return controller_->event_handler_.get();
}

aura::Window* WorkspaceControllerTestApi::GetBackdropWindow() {
  return controller_->layout_manager()->backdrop_controller_->backdrop_window_;
}

}  // namespace ash
