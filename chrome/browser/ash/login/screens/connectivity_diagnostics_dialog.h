// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_CONNECTIVITY_DIAGNOSTICS_DIALOG_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_CONNECTIVITY_DIAGNOSTICS_DIALOG_H_

#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "ui/gfx/native_widget_types.h"

namespace ash {

class ConnectivityDiagnosticsDialog : public SystemWebDialogDelegate {
 public:
  static void ShowDialog(gfx::NativeWindow parent);

 protected:
  ConnectivityDiagnosticsDialog();
  ~ConnectivityDiagnosticsDialog() override;

  ConnectivityDiagnosticsDialog(const ConnectivityDiagnosticsDialog&) = delete;
  ConnectivityDiagnosticsDialog& operator=(
      const ConnectivityDiagnosticsDialog&) = delete;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_CONNECTIVITY_DIAGNOSTICS_DIALOG_H_
