// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/encryption_helper.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

ServerPassphraseTypeChecker::ServerPassphraseTypeChecker(
    syncer::PassphraseType expected_passphrase_type)
    : expected_passphrase_type_(expected_passphrase_type) {}

bool ServerPassphraseTypeChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for a Nigori node with the proper passphrase type to become "
         "available on the server.";

  std::vector<sync_pb::SyncEntity> nigori_entities =
      fake_server()->GetPermanentSyncEntitiesByModelType(syncer::NIGORI);
  EXPECT_LE(nigori_entities.size(), 1U);
  return !nigori_entities.empty() &&
         syncer::ProtoPassphraseInt32ToEnum(
             nigori_entities[0].specifics().nigori().passphrase_type()) ==
             expected_passphrase_type_;
}

ServerNigoriKeyNameChecker::ServerNigoriKeyNameChecker(
    const std::string& expected_key_name)
    : expected_key_name_(expected_key_name) {}

bool ServerNigoriKeyNameChecker::IsExitConditionSatisfied(std::ostream* os) {
  std::vector<sync_pb::SyncEntity> nigori_entities =
      fake_server()->GetPermanentSyncEntitiesByModelType(syncer::NIGORI);
  DCHECK_EQ(nigori_entities.size(), 1U);

  const std::string given_key_name =
      nigori_entities[0].specifics().nigori().encryption_keybag().key_name();

  *os << "Waiting for a Nigori node with proper key bag encryption key name ("
      << expected_key_name_ << ") to become available on the server."
      << "The server key bag encryption key name is " << given_key_name << ".";
  return given_key_name == expected_key_name_;
}

PassphraseRequiredChecker::PassphraseRequiredChecker(
    syncer::SyncServiceImpl* service)
    : SingleClientStatusChangeChecker(service) {}

bool PassphraseRequiredChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Checking whether passhrase is required";
  return service()->GetUserSettings()->IsPassphraseRequired();
}

PassphraseAcceptedChecker::PassphraseAcceptedChecker(
    syncer::SyncServiceImpl* service)
    : SingleClientStatusChangeChecker(service) {}

bool PassphraseAcceptedChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Checking whether passhrase is accepted";
  switch (service()->GetUserSettings()->GetPassphraseType()) {
    case syncer::PassphraseType::kKeystorePassphrase:
    case syncer::PassphraseType::kTrustedVaultPassphrase:
      return false;
    // With kImplicitPassphrase user needs to enter the passphrase even despite
    // it's not treat as explicit passphrase.
    case syncer::PassphraseType::kImplicitPassphrase:
    case syncer::PassphraseType::kFrozenImplicitPassphrase:
    case syncer::PassphraseType::kCustomPassphrase:
      break;
  }
  return !service()->GetUserSettings()->IsPassphraseRequired();
}

TrustedVaultKeyRequiredStateChecker::TrustedVaultKeyRequiredStateChecker(
    syncer::SyncServiceImpl* service,
    bool desired_state)
    : SingleClientStatusChangeChecker(service), desired_state_(desired_state) {}

bool TrustedVaultKeyRequiredStateChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting until trusted vault keys are " +
             std::string(desired_state_ ? "required" : "not required");
  return service()
             ->GetUserSettings()
             ->IsTrustedVaultKeyRequiredForPreferredDataTypes() ==
         desired_state_;
}

TrustedVaultKeysChangedStateChecker::TrustedVaultKeysChangedStateChecker(
    syncer::SyncServiceImpl* service)
    : service_(service), keys_changed_(false) {
  service->GetSyncClientForTest()->GetTrustedVaultClient()->AddObserver(this);
}

TrustedVaultKeysChangedStateChecker::~TrustedVaultKeysChangedStateChecker() {
  service_->GetSyncClientForTest()->GetTrustedVaultClient()->RemoveObserver(
      this);
}

bool TrustedVaultKeysChangedStateChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for trusted vault keys change";
  return keys_changed_;
}

void TrustedVaultKeysChangedStateChecker::OnTrustedVaultKeysChanged() {
  keys_changed_ = true;
  CheckExitCondition();
}

void TrustedVaultKeysChangedStateChecker::
    OnTrustedVaultRecoverabilityChanged() {}
