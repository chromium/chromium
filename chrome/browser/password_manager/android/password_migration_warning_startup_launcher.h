// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MIGRATION_WARNING_STARTUP_LAUNCHER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MIGRATION_WARNING_STARTUP_LAUNCHER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "content/public/browser/web_contents.h"

// Helper class used to launch the password migration warning on startup.
// It ensures that the warning is only shown once in this context and that
// it's only shown if there are passwords saved in the password store.
class PasswordMigrationWarningStartupLauncher
    : public password_manager::PasswordStoreConsumer {
 public:
  using ShowMigrationWarningCallback = base::OnceCallback<void(
      gfx::NativeWindow,
      Profile*,
      password_manager::metrics_util::PasswordMigrationWarningTriggers)>;
  PasswordMigrationWarningStartupLauncher(
      content::WebContents* web_contents,
      Profile* profile,
      ShowMigrationWarningCallback show_migration_warning_callback);

  PasswordMigrationWarningStartupLauncher(
      const PasswordMigrationWarningStartupLauncher&) = delete;
  PasswordMigrationWarningStartupLauncher& operator=(
      const PasswordMigrationWarningStartupLauncher&) = delete;

  ~PasswordMigrationWarningStartupLauncher() override;

  // If a warning can be shown, this fetches passwords from the store
  // which is a first step in showing the warning.
  void MaybeFetchPasswordsAndShowWarning(
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
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<Profile> profile_;

  // Callback that will be invoked to trigger the warning UI. Used to facilitate
  // testing.
  ShowMigrationWarningCallback show_migration_warning_callback_;

  base::WeakPtrFactory<PasswordMigrationWarningStartupLauncher>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MIGRATION_WARNING_STARTUP_LAUNCHER_H_
