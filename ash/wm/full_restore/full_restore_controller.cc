// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/full_restore/full_restore_controller.h"

#include "ash/shell.h"
#include "ash/wm/full_restore/full_restore_window_manager.h"
#include "base/check_op.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

FullRestoreController* g_instance = nullptr;

}  // namespace

FullRestoreController::FullRestoreController()
    : full_restore_window_manager_(
          std::make_unique<FullRestoreWindowManager>()) {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;

  tablet_mode_observeration_.Observe(Shell::Get()->tablet_mode_controller());
}

FullRestoreController::~FullRestoreController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FullRestoreController* FullRestoreController::Get() {
  DCHECK(g_instance);
  return g_instance;
}

void FullRestoreController::SaveWindows() {
  ++save_windows_count_for_testing_;

  // TODO(crbug.com/1164472): Hook up to full_restore::FullRestoreSaveHandler.
}

void FullRestoreController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  // TODO(crbug.com/1164472): Register and the check the pref service and call
  // FullRestoreWindowManager::SetEnabled accordingly.
}

void FullRestoreController::OnTabletModeStarted() {
  SaveWindows();
}

void FullRestoreController::OnTabletModeEnded() {
  SaveWindows();
}

void FullRestoreController::OnTabletControllerDestroyed() {
  tablet_mode_observeration_.Reset();
}

}  // namespace ash
