// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/os_install_screen.h"

#include "chrome/browser/ui/webui/chromeos/login/os_install_screen_handler.h"

namespace chromeos {

OsInstallScreen::OsInstallScreen(OsInstallScreenView* view)
    : BaseScreen(OsInstallScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view) {
  if (view_)
    view_->Bind(this);
}

OsInstallScreen::~OsInstallScreen() {
  if (view_)
    view_->Unbind();
}

void OsInstallScreen::OnViewDestroyed(OsInstallScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void OsInstallScreen::ShowImpl() {
  if (!view_)
    return;

  view_->Show();
}

void OsInstallScreen::HideImpl() {}

}  // namespace chromeos
