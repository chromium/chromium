// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_X_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_X_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_store_default.h"
#include "components/prefs/pref_member.h"

namespace password_manager {
class LoginDatabase;
}

// PasswordStoreX is used on Linux and other non-Windows, non-Mac OS X operating
// systems. It is used as a proxy for the PasswordStoreDefault that basically
// takes care of migrating the passwords of the users to login database. Once
// all users are migrated we should delete this class.
class PasswordStoreX : public password_manager::PasswordStoreDefault {
 public:
  // The state of the migration from native backends and an unencrypted loginDB
  // to an encrypted loginDB.
  enum MigrationToLoginDBStep {
    // Neither started nor failed.
    NOT_ATTEMPTED = 0,
    // The last attempt was not completed.
    DEPRECATED_FAILED,
    // All the data is in the temporary encrypted loginDB.
    DEPRECATED_COPIED_ALL,
    // The standard login database is encrypted.
    LOGIN_DB_REPLACED,
    // The migration is about to be attempted. This value was deprecated and
    // replaced by more price entries. It may still be store in users'
    // preferences.
    STARTED,
    // No access to the native backend.
    POSTPONED,
    // Could not create or write into the file.
    DEPRECATED_FAILED_CREATE_ENCRYPTED,
    // Could not read from the native backend.
    DEPRECATED_FAILED_ACCESS_NATIVE,
    // Could not replace old database.
    FAILED_REPLACE,
    // Could not initialise the encrypted database.
    FAILED_INIT_ENCRYPTED,
    // Could not reset the encrypted database.
    DEPRECATED_FAILED_RECREATE_ENCRYPTED,
    // Could not add entries into the encrypted database.
    FAILED_WRITE_TO_ENCRYPTED,
  };

  PasswordStoreX(std::unique_ptr<password_manager::LoginDatabase> login_db,
                 PrefService* prefs);

  // RefcountedKeyedService:
  void ShutdownOnUIThread() override;

 protected:
  // Implements PasswordStoreSync interface.
  password_manager::FormRetrievalResult ReadAllLogins(
      password_manager::PrimaryKeyToFormMap* key_to_form_map) override;
  password_manager::PasswordStoreChangeList RemoveLoginByPrimaryKeySync(
      int primary_key) override;
  password_manager::PasswordStoreSync::MetadataStore* GetMetadataStore()
      override;

 private:
  ~PasswordStoreX() override;

  // Implements PasswordStore interface.
  scoped_refptr<base::SequencedTaskRunner> CreateBackgroundTaskRunner()
      const override;
  password_manager::PasswordStoreChangeList AddLoginImpl(
      const autofill::PasswordForm& form,
      password_manager::AddLoginError* error = nullptr) override;
  password_manager::PasswordStoreChangeList UpdateLoginImpl(
      const autofill::PasswordForm& form,
      password_manager::UpdateLoginError* error = nullptr) override;
  password_manager::PasswordStoreChangeList RemoveLoginImpl(
      const autofill::PasswordForm& form) override;
  password_manager::PasswordStoreChangeList RemoveLoginsByURLAndTimeImpl(
      const base::Callback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end) override;
  password_manager::PasswordStoreChangeList RemoveLoginsCreatedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) override;
  password_manager::PasswordStoreChangeList DisableAutoSignInForOriginsImpl(
      const base::Callback<bool(const GURL&)>& origin_filter) override;
  std::vector<std::unique_ptr<autofill::PasswordForm>> FillMatchingLogins(
      const FormDigest& form) override;
  std::vector<std::unique_ptr<autofill::PasswordForm>>
  FillMatchingLoginsByPassword(
      const base::string16& plain_text_password) override;
  bool FillAutofillableLogins(
      std::vector<std::unique_ptr<autofill::PasswordForm>>* forms) override;
  bool FillBlacklistLogins(
      std::vector<std::unique_ptr<autofill::PasswordForm>>* forms) override;

  // Checks whether the login database is encrypted or not.
  void CheckMigration();

  // Update |migration_to_login_db_step_| and |migration_step_pref_|.
  void UpdateMigrationToLoginDBStep(MigrationToLoginDBStep step);

  // Update |migration_step_pref_|. It must be executed on the preference's
  // thread.
  void UpdateMigrationPref(MigrationToLoginDBStep step);

  // Whether we have already attempted migration to the native store.
  bool migration_checked_;
  // Tracks the last completed step in the migration from the native backends to
  // LoginDB.
  IntegerPrefMember migration_step_pref_;
  MigrationToLoginDBStep migration_to_login_db_step_ = NOT_ATTEMPTED;

  base::WeakPtrFactory<PasswordStoreX> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreX);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_X_H_
