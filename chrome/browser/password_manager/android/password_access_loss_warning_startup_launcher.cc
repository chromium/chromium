// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_access_loss_warning_startup_launcher.h"

#include "base/notreached.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"

PasswordAccessLossWarningStartupLauncher::
    PasswordAccessLossWarningStartupLauncher(
        ShowAccessLossWarningCallback show_access_loss_warning_callback)
    : show_access_loss_warning_callback_(
          std::move(show_access_loss_warning_callback)) {}

PasswordAccessLossWarningStartupLauncher::
    ~PasswordAccessLossWarningStartupLauncher() = default;

void PasswordAccessLossWarningStartupLauncher::FetchPasswordsAndShowWarning(
    password_manager::PasswordStoreInterface* store) {
  store->GetAllLogins(weak_ptr_factory_.GetWeakPtr());
}

void PasswordAccessLossWarningStartupLauncher::
    OnGetPasswordStoreResultsOrErrorFrom(
        password_manager::PasswordStoreInterface* store,
        password_manager::LoginsResultOrError results_or_error) {
  if (absl::holds_alternative<password_manager::PasswordStoreBackendError>(
          results_or_error)) {
    return;
  }
  password_manager::LoginsResult passwords =
      std::move(absl::get<password_manager::LoginsResult>(results_or_error));
  if (passwords.empty()) {
    return;
  }
  std::move(show_access_loss_warning_callback_).Run();
}

void PasswordAccessLossWarningStartupLauncher::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> results) {
  NOTREACHED();
}
