// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/shimless_rma/chrome_shimless_rma_delegate.h"

#include "base/command_line.h"
#include "chrome/browser/ash/login/chrome_restart_request.h"
#include "chrome/browser/ui/webui/chromeos/diagnostics_dialog.h"

namespace ash {
namespace shimless_rma {

ChromeShimlessRmaDelegate::ChromeShimlessRmaDelegate() = default;
ChromeShimlessRmaDelegate::~ChromeShimlessRmaDelegate() = default;

void ChromeShimlessRmaDelegate::RestartChrome() {
  // TODO(gavinwill): Add the option to pass the --no-rma flag when implemented.
  ash::RestartChrome(*base::CommandLine::ForCurrentProcess(),
                     ash::RestartChromeReason::kUserless);
}

void ChromeShimlessRmaDelegate::ShowDiagnosticsDialog() {
  chromeos::DiagnosticsDialog::ShowDialog();
}

}  // namespace shimless_rma
}  // namespace ash
