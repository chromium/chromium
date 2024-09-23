// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_window_resizer_test_api.h"

#include "ash/wm/workspace/phantom_window_controller.h"
#include "ash/wm/workspace/workspace_window_resizer.h"

namespace ash {

WorkspaceWindowResizerTestApi::WorkspaceWindowResizerTestApi() = default;

WorkspaceWindowResizerTestApi::~WorkspaceWindowResizerTestApi() = default;

PhantomWindowController*
WorkspaceWindowResizerTestApi::GetSnapPhantomWindowController() {
  auto* workspace_resizer = WorkspaceWindowResizer::GetInstanceForTest();
  CHECK(workspace_resizer);
  return workspace_resizer->snap_phantom_window_controller_.get();
}

base::OneShotTimer& WorkspaceWindowResizerTestApi::GetDwellCountdownTimer()
    const {
  auto* workspace_resizer = WorkspaceWindowResizer::GetInstanceForTest();
  CHECK(workspace_resizer);
  return workspace_resizer->dwell_countdown_timer_;
}

}  // namespace ash
