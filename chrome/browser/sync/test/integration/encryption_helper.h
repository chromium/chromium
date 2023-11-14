// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_ENCRYPTION_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_ENCRYPTION_HELPER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/test/fake_server.h"
#include "components/trusted_vault/trusted_vault_client.h"

// Checker used to block until a Nigori with a given passphrase type is
// available on the server.
class ServerPassphraseTypeChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  explicit ServerPassphraseTypeChecker(
      syncer::PassphraseType expected_passphrase_type);

  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const syncer::PassphraseType expected_passphrase_type_;
};

// Checker used to block until a Nigori populated with cross-user sharing keys
// is available on the server.
class CrossUserSharingKeysChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  CrossUserSharingKeysChecker();

  bool IsExitConditionSatisfied(std::ostream* os) override;
};

// Checker used to block until a Nigori with a given keybag encryption key name
// is available on the server.
class ServerNigoriKeyNameChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  explicit ServerNigoriKeyNameChecker(const std::string& expected_key_name);

  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const std::string expected_key_name_;
};

// Checker to block until service is waiting for a passphrase.
class PassphraseRequiredChecker : public SingleClientStatusChangeChecker {
 public:
  explicit PassphraseRequiredChecker(syncer::SyncServiceImpl* service);

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;
};

// Checker to block until service has accepted a new passphrase.
class PassphraseAcceptedChecker : public SingleClientStatusChangeChecker {
 public:
  explicit PassphraseAcceptedChecker(syncer::SyncServiceImpl* service);

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;
};

// Checker to block until service has finished setting up a given passphrase
// type.
class PassphraseTypeChecker : public SingleClientStatusChangeChecker {
 public:
  PassphraseTypeChecker(syncer::SyncServiceImpl* service,
                        syncer::PassphraseType expected_passphrase_type);

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const syncer::PassphraseType expected_passphrase_type_;
};

// Checker used to block until Sync requires or stops requiring trusted vault
// keys.
class TrustedVaultKeyRequiredStateChecker
    : public SingleClientStatusChangeChecker {
 public:
  TrustedVaultKeyRequiredStateChecker(syncer::SyncServiceImpl* service,
                                      bool desired_state);

  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const bool desired_state_;
};

// Checker used to block until trusted vault keys are changed.
class TrustedVaultKeysChangedStateChecker
    : public StatusChangeChecker,
      trusted_vault::TrustedVaultClient::Observer {
 public:
  explicit TrustedVaultKeysChangedStateChecker(
      syncer::SyncServiceImpl* service);
  ~TrustedVaultKeysChangedStateChecker() override;

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // TrustedVaultClient::Observer overrides.
  void OnTrustedVaultKeysChanged() override;
  void OnTrustedVaultRecoverabilityChanged() override;

 private:
  const raw_ptr<syncer::SyncServiceImpl> service_;
  bool keys_changed_ = false;
};

// Used to wait until IsTrustedVaultRecoverabilityDegraded() returns the desired
// value.
class TrustedVaultRecoverabilityDegradedStateChecker
    : public SingleClientStatusChangeChecker {
 public:
  TrustedVaultRecoverabilityDegradedStateChecker(
      syncer::SyncServiceImpl* service,
      bool degraded);
  ~TrustedVaultRecoverabilityDegradedStateChecker() override = default;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const bool degraded_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_ENCRYPTION_HELPER_H_
