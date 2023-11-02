// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/shimless_rma/chrome_shimless_rma_delegate.h"

#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/login/chrome_restart_request.h"
#include "chrome/browser/ash/system/device_disabling_manager.h"
#include "chrome/browser/ui/webui/chromeos/diagnostics_dialog.h"

namespace ash {
namespace shimless_rma {

ChromeShimlessRmaDelegate::ChromeShimlessRmaDelegate() = default;
ChromeShimlessRmaDelegate::~ChromeShimlessRmaDelegate() = default;

void ChromeShimlessRmaDelegate::ExitRmaThenRestartChrome() {
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  base::CommandLine command_line(browser_command_line);
  command_line.AppendSwitch(::ash::switches::kRmaNotAllowed);
  // Remove any attempts to launch RMA.
  command_line.RemoveSwitch(::ash::switches::kLaunchRma);
  ash::RestartChrome(command_line, ash::RestartChromeReason::kUserless);
}

void ChromeShimlessRmaDelegate::ShowDiagnosticsDialog() {
  // Don't launch Diagnostics if device is disabled.
  if (system::DeviceDisablingManager::IsDeviceDisabledDuringNormalOperation()) {
    return;
  }

  chromeos::DiagnosticsDialog::ShowDialog();
}

void ChromeShimlessRmaDelegate::RefreshAccessibilityManagerProfile() {
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  DCHECK(accessibility_manager);
  accessibility_manager->OnShimlessRmaLaunched();
}

}  // namespace shimless_rma
}  // namespace ash
