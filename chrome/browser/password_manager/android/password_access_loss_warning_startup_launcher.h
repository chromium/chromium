// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_ACCESS_LOSS_WARNING_STARTUP_LAUNCHER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_ACCESS_LOSS_WARNING_STARTUP_LAUNCHER_H_

#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"

// Helper class used to launch the password access loss warning on startup.
// It ensures that the warning is only shown if there are passwords saved in the
// password store.
class PasswordAccessLossWarningStartupLauncher
    : public password_manager::PasswordStoreConsumer {
 public:
  using ShowAccessLossWarningCallback = base::OnceCallback<void()>;
  explicit PasswordAccessLossWarningStartupLauncher(
      ShowAccessLossWarningCallback show_migration_warning_callback);

  PasswordAccessLossWarningStartupLauncher(
      const PasswordAccessLossWarningStartupLauncher&) = delete;
  PasswordAccessLossWarningStartupLauncher& operator=(
      const PasswordAccessLossWarningStartupLauncher&) = delete;

  ~PasswordAccessLossWarningStartupLauncher() override;

  // This fetches passwords from the store which is a first step in showing the
  // warning.
  void FetchPasswordsAndShowWarning(
      password_manager::PasswordStoreInterface* store);

 private:
  // Receives a result from the password store. If there are any saved passwords
  // and all other conditions for showing the warning are met it will display
  // it.
  void OnGetPasswordStoreResultsOrErrorFrom(
      password_manager::PasswordStoreInterface* store,
      password_manager::LoginsResultOrError results_or_error) override;

  // Not implemented. Required override.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<password_manager::PasswordForm>>) override;

  // Callback that will be invoked to trigger the warning UI. Used to facilitate
  // testing.
  ShowAccessLossWarningCallback show_access_loss_warning_callback_;

  base::WeakPtrFactory<PasswordAccessLossWarningStartupLauncher>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_ACCESS_LOSS_WARNING_STARTUP_LAUNCHER_H_
