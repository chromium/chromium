// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_migration_warning_startup_launcher.h"

#include "base/notreached.h"
#include "chrome/browser/password_manager/android/local_passwords_migration_warning_util.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

PasswordMigrationWarningStartupLauncher::
    PasswordMigrationWarningStartupLauncher(
        content::WebContents* web_contents,
        Profile* profile,
        ShowMigrationWarningCallback show_migration_warning_callback)
    : web_contents_(web_contents),
      profile_(profile),
      show_migration_warning_callback_(
          std::move(show_migration_warning_callback)) {}

PasswordMigrationWarningStartupLauncher::
    ~PasswordMigrationWarningStartupLauncher() = default;

void PasswordMigrationWarningStartupLauncher::MaybeFetchPasswordsAndShowWarning(
    password_manager::PasswordStoreInterface* store) {
  bool local_migration_warning_shown_at_startup =
      profile_->GetPrefs()->GetBoolean(
          password_manager::prefs::
              kLocalPasswordMigrationWarningShownAtStartup);
  if (local_migration_warning_shown_at_startup) {
    return;
  }
  if (!local_password_migration::ShouldShowWarning(profile_)) {
    return;
  }
  store->GetAutofillableLogins(weak_ptr_factory_.GetWeakPtr());
}

void PasswordMigrationWarningStartupLauncher::
    OnGetPasswordStoreResultsOrErrorFrom(
        password_manager::PasswordStoreInterface* store,
        FormsOrError results_or_error) {
  if (absl::holds_alternative<password_manager::PasswordStoreBackendError>(
          results_or_error)) {
    return;
  }
  password_manager::LoginsResult passwords =
      std::move(absl::get<password_manager::LoginsResult>(results_or_error));
  if (passwords.empty()) {
    return;
  }

  if (local_password_migration::ShouldShowWarning(profile_) &&
      show_migration_warning_callback_) {
    std::move(show_migration_warning_callback_)
        .Run(web_contents_->GetTopLevelNativeWindow(), profile_,
             password_manager::metrics_util::PasswordMigrationWarningTriggers::
                 kChromeStartup);
    PrefService* prefs = profile_->GetPrefs();
    prefs->SetBoolean(
        password_manager::prefs::kLocalPasswordMigrationWarningShownAtStartup,
        true);
  }
}

void PasswordMigrationWarningStartupLauncher::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> results) {
  NOTREACHED();
}
