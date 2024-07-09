// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_WINDOW_RESIZER_TEST_API_H_
#define ASH_WM_WORKSPACE_WORKSPACE_WINDOW_RESIZER_TEST_API_H_

#include "base/timer/timer.h"

namespace ash {

class PhantomWindowController;

class WorkspaceWindowResizerTestApi {
 public:
  WorkspaceWindowResizerTestApi();
  WorkspaceWindowResizerTestApi(const WorkspaceWindowResizerTestApi&) = delete;
  WorkspaceWindowResizerTestApi& operator=(
      const WorkspaceWindowResizerTestApi&) = delete;
  ~WorkspaceWindowResizerTestApi();

  PhantomWindowController* GetSnapPhantomWindowController();
  base::OneShotTimer& GetDwellCountdownTimer() const;
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_WINDOW_RESIZER_TEST_API_H_
