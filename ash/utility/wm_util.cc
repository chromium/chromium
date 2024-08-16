// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/wm_util.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "components/session_manager/session_manager_types.h"

namespace ash_util {

void SetupWidgetInitParamsForContainer(views::Widget::InitParams* params,
                                       int container_id) {
  DCHECK_GE(container_id, ash::kShellWindowId_MinContainer);
  DCHECK_LE(container_id, ash::kShellWindowId_MaxContainer);

  params->parent = ash::Shell::GetContainer(
      ash::Shell::GetRootWindowForNewWindows(), container_id);
}

void SetupWidgetInitParamsForContainerInPrimary(
    views::Widget::InitParams* params,
    int container_id) {
  DCHECK_GE(container_id, ash::kShellWindowId_MinContainer);
  DCHECK_LE(container_id, ash::kShellWindowId_MaxContainer);

  params->parent = ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                            container_id);
}

int GetSystemModalDialogContainerId() {
  return ash::Shell::Get()->session_controller()->GetSessionState() ==
                 session_manager::SessionState::ACTIVE
             ? ash::kShellWindowId_SystemModalContainer
             : ash::kShellWindowId_LockSystemModalContainer;
}

}  // namespace ash_util
