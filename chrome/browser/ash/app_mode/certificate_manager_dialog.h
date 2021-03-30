// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_CERTIFICATE_MANAGER_DIALOG_H_
#define CHROME_BROWSER_ASH_APP_MODE_CERTIFICATE_MANAGER_DIALOG_H_

#include "base/macros.h"
#include "chrome/browser/ash/login/ui/login_web_dialog.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace ash {

// This dialog is used to manage user certificates from the kiosk launch screen.
class CertificateManagerDialog : public LoginWebDialog {
 public:
  CertificateManagerDialog(Profile* profile,
                           LoginWebDialog::Delegate* delegate,
                           gfx::NativeWindow window);
  ~CertificateManagerDialog() override;

  // ui::WebDialogDelegate
  std::u16string GetDialogTitle() const override;
  void GetDialogSize(gfx::Size* size) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CertificateManagerDialog);
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the //chrome/browser/chromeos
// source code migration is finished.
namespace chromeos {
using ::ash::CertificateManagerDialog;
}

#endif  // CHROME_BROWSER_ASH_APP_MODE_CERTIFICATE_MANAGER_DIALOG_H_
