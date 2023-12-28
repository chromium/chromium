// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_CONTROLLER_TEST_API_H_
#define ASH_WM_WORKSPACE_CONTROLLER_TEST_API_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"

namespace aura {
class Window;
}

namespace ash {
class WorkspaceController;
class WorkspaceEventHandler;

class ASH_EXPORT WorkspaceControllerTestApi {
 public:
  explicit WorkspaceControllerTestApi(WorkspaceController* controller);
  WorkspaceControllerTestApi(const WorkspaceControllerTestApi&) = delete;
  WorkspaceControllerTestApi& operator=(const WorkspaceControllerTestApi&) =
      delete;
  ~WorkspaceControllerTestApi();

  WorkspaceEventHandler* GetEventHandler();
  aura::Window* GetBackdropWindow();

 private:
  raw_ptr<WorkspaceController> controller_;
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_CONTROLLER_TEST_API_H_
