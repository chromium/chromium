// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_H_

#include <memory>

#include "base/macros.h"
#include "components/password_manager/core/browser/keychain_migration_status_mac.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_default.h"
#include "components/prefs/pref_member.h"

namespace password_manager {
class LoginDatabase;
}

// Password store for Mac.
class PasswordStoreMac : public password_manager::PasswordStoreDefault {
 public:
  PasswordStoreMac(std::unique_ptr<password_manager::LoginDatabase> login_db,
                   PrefService* prefs);

  // PasswordStore:
  bool Init(const syncer::SyncableService::StartSyncFlare& flare,
            PrefService* prefs) override;
  void ShutdownOnUIThread() override;

#if defined(UNIT_TEST)
  password_manager::LoginDatabase* login_metadata_db() { return login_db(); }
#endif

 private:
  ~PasswordStoreMac() override;

  // PasswordStore:
  bool InitOnBackgroundSequence(
      const syncer::SyncableService::StartSyncFlare& flare) override;

  // Writes status to the prefs.
  void UpdateStatusPref(password_manager::MigrationStatus status);

  // Bound to the pref containing migration status for the profile.
  IntegerPrefMember migration_status_;

  // Initial migration status when the class is initialized.
  password_manager::MigrationStatus initial_status_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreMac);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_H_
