// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/connectivity_diagnostics_dialog.h"

#include <string>

#include "ash/webui/connectivity_diagnostics/url_constants.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/native_widget_types.h"

namespace {

// Scale factor for size of the connectivity diagnostics dialog, based on
// display size.
const float kConnectivityDiagnosticsDialogScale = .8;

}  // namespace

namespace ash {

// static
void ConnectivityDiagnosticsDialog::ShowDialog(gfx::NativeWindow parent) {
  ConnectivityDiagnosticsDialog* dialog = new ConnectivityDiagnosticsDialog();
  dialog->ShowSystemDialog(parent);
}

ConnectivityDiagnosticsDialog::ConnectivityDiagnosticsDialog()
    : SystemWebDialogDelegate(GURL(kChromeUIConnectivityDiagnosticsUrl),
                              /*title=*/std::u16string()) {}

ConnectivityDiagnosticsDialog::~ConnectivityDiagnosticsDialog() = default;

void ConnectivityDiagnosticsDialog::GetDialogSize(gfx::Size* size) const {
  const display::Display display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  *size =
      gfx::Size(display.size().width() * kConnectivityDiagnosticsDialogScale,
                display.size().height() * kConnectivityDiagnosticsDialogScale);
}

}  // namespace ash
