// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_CERTIFICATE_MANAGER_DIALOG_H_
#define CHROME_BROWSER_ASH_APP_MODE_CERTIFICATE_MANAGER_DIALOG_H_

#include "chrome/browser/ash/login/ui/login_web_dialog.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace ash {

// This dialog is used to manage user certificates from the kiosk launch screen.
class CertificateManagerDialog : public LoginWebDialog {
 public:
  CertificateManagerDialog(Profile* profile, gfx::NativeWindow window);
  CertificateManagerDialog(const CertificateManagerDialog&) = delete;
  CertificateManagerDialog& operator=(const CertificateManagerDialog&) = delete;
  ~CertificateManagerDialog() override;

  // ui::WebDialogDelegate
  std::u16string GetDialogTitle() const override;
  void GetDialogSize(gfx::Size* size) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_CERTIFICATE_MANAGER_DIALOG_H_
