// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_x.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_service.h"

using autofill::PasswordForm;
using password_manager::PasswordStoreChange;
using password_manager::PasswordStoreChangeList;
using password_manager::PasswordStoreDefault;

namespace {
// Returns the password_manager::metrics_util::LinuxBackendMigrationStatus
// equivalent for |step|.
password_manager::metrics_util::LinuxBackendMigrationStatus StepForMetrics(
    PasswordStoreX::MigrationToLoginDBStep step) {
  using password_manager::metrics_util::LinuxBackendMigrationStatus;
  switch (step) {
    case PasswordStoreX::NOT_ATTEMPTED:
      return LinuxBackendMigrationStatus::kNotAttempted;
    case PasswordStoreX::DEPRECATED_FAILED:
      return LinuxBackendMigrationStatus::kDeprecatedFailed;
    case PasswordStoreX::DEPRECATED_COPIED_ALL:
      return LinuxBackendMigrationStatus::kDeprecatedCopiedAll;
    case PasswordStoreX::LOGIN_DB_REPLACED:
      return LinuxBackendMigrationStatus::kLoginDBReplaced;
    case PasswordStoreX::STARTED:
      return LinuxBackendMigrationStatus::kStarted;
    case PasswordStoreX::POSTPONED:
      return LinuxBackendMigrationStatus::kPostponed;
    case PasswordStoreX::DEPRECATED_FAILED_CREATE_ENCRYPTED:
      return LinuxBackendMigrationStatus::kDeprecatedFailedCreatedEncrypted;
    case PasswordStoreX::DEPRECATED_FAILED_ACCESS_NATIVE:
      return LinuxBackendMigrationStatus::kDeprecatedFailedAccessNative;
    case PasswordStoreX::FAILED_REPLACE:
      return LinuxBackendMigrationStatus::kFailedReplace;
    case PasswordStoreX::FAILED_INIT_ENCRYPTED:
      return LinuxBackendMigrationStatus::kFailedInitEncrypted;
    case PasswordStoreX::DEPRECATED_FAILED_RECREATE_ENCRYPTED:
      return LinuxBackendMigrationStatus::kDeprecatedFailedRecreateEncrypted;
    case PasswordStoreX::FAILED_WRITE_TO_ENCRYPTED:
      return LinuxBackendMigrationStatus::kFailedWriteToEncrypted;
  }
  NOTREACHED();
  return LinuxBackendMigrationStatus::kNotAttempted;
}

// Reads and rewrites every entry in |login_db|, reading unecrypted and writing
// encrypted. This should run in a transaction. Returns LOGIN_DB_REPLACED on
// success, otherwise the step where the process failed.
PasswordStoreX::MigrationToLoginDBStep EncryptDatabase(
    password_manager::LoginDatabase* login_db) {
  // Read the unencrypted entries.
  login_db->disable_encryption();
  std::map<int, std::unique_ptr<autofill::PasswordForm>> logins;
  if (login_db->GetAllLogins(&logins) !=
      password_manager::FormRetrievalResult::kSuccess) {
    return PasswordStoreX::FAILED_INIT_ENCRYPTED;
  }

  // Re-insert the entries, encrypted this time.
  login_db->enable_encryption();
  for (const auto& password_form : logins) {
    password_manager::UpdateLoginError error;
    PasswordStoreChangeList changes =
        login_db->UpdateLogin(*password_form.second, &error);
    // Check that an existing entry was replaced.
    if (error != password_manager::UpdateLoginError::kNone || changes.empty() ||
        changes[0].type() != password_manager::PasswordStoreChange::UPDATE) {
      return PasswordStoreX::FAILED_WRITE_TO_ENCRYPTED;
    }
  }

  return PasswordStoreX::LOGIN_DB_REPLACED;
}

// Reads and rewrites every entry in |login_db|, reading unecrypted and writing
// encrypted. Returns LOGIN_DB_REPLACED on success, otherwise the step where the
// process failed.
PasswordStoreX::MigrationToLoginDBStep MigrateUnencryptedDatabase(
    password_manager::LoginDatabase* login_db) {
  if (!login_db->BeginTransaction())
    return PasswordStoreX::POSTPONED;

  PasswordStoreX::MigrationToLoginDBStep encryption_result =
      EncryptDatabase(login_db);

  if (encryption_result == PasswordStoreX::LOGIN_DB_REPLACED) {
    if (!login_db->CommitTransaction()) {
      return PasswordStoreX::FAILED_REPLACE;
    }
  } else {
    login_db->RollbackTransaction();
  }

  return encryption_result;
}

}  // namespace

PasswordStoreX::PasswordStoreX(
    std::unique_ptr<password_manager::LoginDatabase> login_db,
    PrefService* prefs)
    : PasswordStoreDefault(std::move(login_db)), migration_checked_(false) {
  migration_step_pref_.Init(password_manager::prefs::kMigrationToLoginDBStep,
                            prefs);
  migration_to_login_db_step_ =
      static_cast<MigrationToLoginDBStep>(migration_step_pref_.GetValue());

  base::UmaHistogramEnumeration(
      "PasswordManager.LinuxBackendMigration.Adoption",
      StepForMetrics(migration_to_login_db_step_));
}

PasswordStoreX::~PasswordStoreX() {}

scoped_refptr<base::SequencedTaskRunner>
PasswordStoreX::CreateBackgroundTaskRunner() const {
  return PasswordStoreDefault::CreateBackgroundTaskRunner();
}

PasswordStoreChangeList PasswordStoreX::AddLoginImpl(
    const PasswordForm& form,
    password_manager::AddLoginError* error) {
  CheckMigration();
  return PasswordStoreDefault::AddLoginImpl(form, error);
}

PasswordStoreChangeList PasswordStoreX::UpdateLoginImpl(
    const PasswordForm& form,
    password_manager::UpdateLoginError* error) {
  CheckMigration();
  return PasswordStoreDefault::UpdateLoginImpl(form, error);
}

PasswordStoreChangeList PasswordStoreX::RemoveLoginImpl(
    const PasswordForm& form) {
  CheckMigration();
  return PasswordStoreDefault::RemoveLoginImpl(form);
}

PasswordStoreChangeList PasswordStoreX::RemoveLoginsByURLAndTimeImpl(
    const base::Callback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end) {
  CheckMigration();
  return PasswordStoreDefault::RemoveLoginsByURLAndTimeImpl(
      url_filter, delete_begin, delete_end);
}

PasswordStoreChangeList PasswordStoreX::RemoveLoginsCreatedBetweenImpl(
    base::Time delete_begin,
    base::Time delete_end) {
  CheckMigration();
  return PasswordStoreDefault::RemoveLoginsCreatedBetweenImpl(delete_begin,
                                                              delete_end);
}

PasswordStoreChangeList PasswordStoreX::DisableAutoSignInForOriginsImpl(
    const base::Callback<bool(const GURL&)>& origin_filter) {
  CheckMigration();
  return PasswordStoreDefault::DisableAutoSignInForOriginsImpl(origin_filter);
}

std::vector<std::unique_ptr<PasswordForm>> PasswordStoreX::FillMatchingLogins(
    const FormDigest& form) {
  CheckMigration();
  return PasswordStoreDefault::FillMatchingLogins(form);
}

std::vector<std::unique_ptr<PasswordForm>>
PasswordStoreX::FillMatchingLoginsByPassword(
    const base::string16& plain_text_password) {
  CheckMigration();
  return PasswordStoreDefault::FillMatchingLoginsByPassword(
      plain_text_password);
}

bool PasswordStoreX::FillAutofillableLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  CheckMigration();
  return PasswordStoreDefault::FillAutofillableLogins(forms);
}

bool PasswordStoreX::FillBlacklistLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  CheckMigration();
  return PasswordStoreDefault::FillBlacklistLogins(forms);
}

void PasswordStoreX::CheckMigration() {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());

  if (migration_checked_)
    return;
  migration_checked_ = true;

  if (migration_to_login_db_step_ == LOGIN_DB_REPLACED) {
    return;
  }

  if (!login_db()) {
    LOG(ERROR) << "Could not start the migration into the encrypted "
                  "LoginDatabase because the database failed to initialise.";
    return;
  }

  // If the db is empty, there are no records to migrate, and we then can call
  // it a completed migration.
  if (login_db()->IsEmpty()) {
    UpdateMigrationToLoginDBStep(LOGIN_DB_REPLACED);
  } else {
    // The user has unencrypted entries in their login database. We will try to
    // encrypt them.
    UpdateMigrationToLoginDBStep(STARTED);
    MigrationToLoginDBStep migration_result =
        MigrateUnencryptedDatabase(login_db());
    UpdateMigrationToLoginDBStep(migration_result);
    // If the migration did not complete, then we will continue using the
    // database without encryption.
    if (migration_result == LOGIN_DB_REPLACED)
      login_db()->enable_encryption();
    else
      login_db()->disable_encryption();
  }

  base::UmaHistogramEnumeration(
      "PasswordManager.LinuxBackendMigration.AttemptResult",
      StepForMetrics(migration_to_login_db_step_));
}

void PasswordStoreX::UpdateMigrationToLoginDBStep(MigrationToLoginDBStep step) {
  migration_to_login_db_step_ = step;
  main_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&PasswordStoreX::UpdateMigrationPref,
                                weak_ptr_factory_.GetWeakPtr(),
                                migration_to_login_db_step_));
}

void PasswordStoreX::UpdateMigrationPref(MigrationToLoginDBStep step) {
  migration_step_pref_.SetValue(step);
}

void PasswordStoreX::ShutdownOnUIThread() {
  migration_step_pref_.Destroy();
  // Invalidate the weak pointer to preempt any posted tasks in
  // UpdateMigrationToLoginDBStep() because they cannot use the
  // |migration_step_pref_| anymore. Both ShutdownOnUIThread() and
  // UpdateMigrationToLoginDBStep() are only executed on the UI thread.
  weak_ptr_factory_.InvalidateWeakPtrs();
  PasswordStoreDefault::ShutdownOnUIThread();
}

password_manager::FormRetrievalResult PasswordStoreX::ReadAllLogins(
    password_manager::PrimaryKeyToFormMap* key_to_form_map) {
  CheckMigration();
  return PasswordStoreDefault::ReadAllLogins(key_to_form_map);
}

PasswordStoreChangeList PasswordStoreX::RemoveLoginByPrimaryKeySync(
    int primary_key) {
  CheckMigration();
  return PasswordStoreDefault::RemoveLoginByPrimaryKeySync(primary_key);
}

password_manager::PasswordStoreSync::MetadataStore*
PasswordStoreX::GetMetadataStore() {
  CheckMigration();
  return PasswordStoreDefault::GetMetadataStore();
}
