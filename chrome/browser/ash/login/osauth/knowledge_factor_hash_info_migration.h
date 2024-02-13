// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OSAUTH_KNOWLEDGE_FACTOR_HASH_INFO_MIGRATION_H_
#define CHROME_BROWSER_ASH_LOGIN_OSAUTH_KNOWLEDGE_FACTOR_HASH_INFO_MIGRATION_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/osauth/auth_factor_migration.h"
#include "chrome/browser/ash/login/quick_unlock/pin_salt_storage.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"

namespace ash {

class UserContext;

// This class checks whether the configured knowledge factors (PIN and password)
// have hash information inside the metadata, and if not, update their metadata.
class KnowledgeFactorHashInfoMigration : public AuthFactorMigration {
 public:
  explicit KnowledgeFactorHashInfoMigration(UserDataAuthClient* user_data_auth);
  ~KnowledgeFactorHashInfoMigration() override;

  KnowledgeFactorHashInfoMigration(const KnowledgeFactorHashInfoMigration&) =
      delete;
  KnowledgeFactorHashInfoMigration& operator=(
      const KnowledgeFactorHashInfoMigration&) = delete;

  // Backfills the hash information for knowledge factors (PIN and password).
  void Run(std::unique_ptr<UserContext> context,
           AuthOperationCallback callback) override;
  bool WasSkipped() override;
  MigrationName GetName() override;

 private:
  void CheckAuthFactorsToUpdate(std::unique_ptr<UserContext> context,
                                AuthOperationCallback callback);
  void OnAuthFactorConfigurationLoaded(
      AuthOperationCallback callback,
      std::unique_ptr<UserContext> context,
      std::optional<AuthenticationError> error);
  void UpdatePasswordFactorMetadata(std::unique_ptr<UserContext> context,
                                    AuthOperationCallback callback,
                                    const cryptohome::KeyLabel& password_label,
                                    bool pin_needs_update,
                                    const std::string& system_salt);
  void OnPasswordFactorMetadataUpdated(
      AuthOperationCallback callback,
      bool pin_needs_update,
      std::unique_ptr<UserContext> context,
      std::optional<AuthenticationError> error);
  void UpdatePinFactorMetadata(std::unique_ptr<UserContext> context,
                               AuthOperationCallback callback);
  void OnPinFactorMetadataUpdated(AuthOperationCallback callback,
                                  std::unique_ptr<UserContext> context,
                                  std::optional<AuthenticationError> error);

  bool was_skipped_ = false;
  std::unique_ptr<AuthFactorEditor> editor_;
  std::unique_ptr<quick_unlock::PinSaltStorage> pin_salt_storage_;
  // Must be the last member.
  base::WeakPtrFactory<KnowledgeFactorHashInfoMigration> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_OSAUTH_KNOWLEDGE_FACTOR_HASH_INFO_MIGRATION_H_
