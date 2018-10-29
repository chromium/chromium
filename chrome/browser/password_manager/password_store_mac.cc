// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_mac.h"
#include "base/metrics/histogram_macros.h"
#include "components/os_crypt/os_crypt.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"

using password_manager::MigrationStatus;

PasswordStoreMac::PasswordStoreMac(
    std::unique_ptr<password_manager::LoginDatabase> login_db,
    PrefService* prefs)
    : PasswordStoreDefault(std::move(login_db)),
      initial_status_(MigrationStatus::NOT_STARTED) {
  migration_status_.Init(password_manager::prefs::kKeychainMigrationStatus,
                         prefs);
}

bool PasswordStoreMac::Init(
    const syncer::SyncableService::StartSyncFlare& flare,
    PrefService* prefs) {
  initial_status_ = static_cast<MigrationStatus>(migration_status_.GetValue());
  return PasswordStoreDefault::Init(flare, prefs);
}

void PasswordStoreMac::ShutdownOnUIThread() {
  PasswordStoreDefault::ShutdownOnUIThread();

  // Unsubscribe the observer, otherwise it's too late in the destructor.
  migration_status_.Destroy();
}

PasswordStoreMac::~PasswordStoreMac() = default;

bool PasswordStoreMac::InitOnBackgroundSequence(
    const syncer::SyncableService::StartSyncFlare& flare) {
  if (!PasswordStoreDefault::InitOnBackgroundSequence(flare))
    return false;

  if (!OSCrypt::IsEncryptionAvailable())
    return false;

  if (login_db() && (initial_status_ == MigrationStatus::NOT_STARTED ||
                     initial_status_ == MigrationStatus::FAILED_ONCE ||
                     initial_status_ == MigrationStatus::FAILED_TWICE)) {
    // Migration isn't possible due to Chrome changing the certificate. Just
    // drop the entries in the DB because they don't have passwords anyway.
    login_db()->RemoveLoginsCreatedBetween(base::Time(), base::Time());
    initial_status_ = MigrationStatus::MIGRATION_STOPPED;
    main_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&PasswordStoreMac::UpdateStatusPref, this, initial_status_));
  }

  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.KeychainMigration.Status",
      static_cast<int>(initial_status_),
      static_cast<int>(MigrationStatus::MIGRATION_STATUS_COUNT));

  return true;
}

void PasswordStoreMac::UpdateStatusPref(MigrationStatus status) {
  // The method can be called after ShutdownOnUIThread().
  if (migration_status_.prefs())
    migration_status_.SetValue(static_cast<int>(status));
}
