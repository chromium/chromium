// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/encryption_migration_screen.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/encryption_migration_screen_handler.h"

namespace chromeos {

EncryptionMigrationScreen::EncryptionMigrationScreen(
    EncryptionMigrationScreenView* view)
    : BaseScreen(EncryptionMigrationScreenView::kScreenId), view_(view) {
  DCHECK(view_);
  if (view_)
    view_->SetDelegate(this);
}

EncryptionMigrationScreen::~EncryptionMigrationScreen() {
  if (view_)
    view_->SetDelegate(nullptr);
}

void EncryptionMigrationScreen::OnViewDestroyed(
    EncryptionMigrationScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void EncryptionMigrationScreen::Show() {
  if (view_)
    view_->Show();
}

void EncryptionMigrationScreen::Hide() {
  if (view_)
    view_->Hide();
}

void EncryptionMigrationScreen::SetUserContext(
    const UserContext& user_context) {
  DCHECK(view_);
  view_->SetUserContext(user_context);
}

void EncryptionMigrationScreen::SetMode(EncryptionMigrationMode mode) {
  DCHECK(view_);
  view_->SetMode(mode);
}

void EncryptionMigrationScreen::SetContinueLoginCallback(
    ContinueLoginCallback callback) {
  DCHECK(view_);
  view_->SetContinueLoginCallback(std::move(callback));
}

void EncryptionMigrationScreen::SetRestartLoginCallback(
    RestartLoginCallback callback) {
  DCHECK(view_);
  view_->SetRestartLoginCallback(std::move(callback));
}

void EncryptionMigrationScreen::SetupInitialView() {
  DCHECK(view_);
  view_->SetupInitialView();
}

}  // namespace chromeos
