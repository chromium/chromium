// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/encryption_helper.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

ServerNigoriChecker::ServerNigoriChecker(
    syncer::ProfileSyncService* service,
    fake_server::FakeServer* fake_server,
    syncer::PassphraseType expected_passphrase_type)
    : SingleClientStatusChangeChecker(service),
      fake_server_(fake_server),
      expected_passphrase_type_(expected_passphrase_type) {}

bool ServerNigoriChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for a Nigori node with the proper passphrase type to become "
         "available on the server.";

  std::vector<sync_pb::SyncEntity> nigori_entities =
      fake_server_->GetPermanentSyncEntitiesByModelType(syncer::NIGORI);
  EXPECT_LE(nigori_entities.size(), 1U);
  return !nigori_entities.empty() &&
         syncer::ProtoPassphraseInt32ToEnum(
             nigori_entities[0].specifics().nigori().passphrase_type()) ==
             expected_passphrase_type_;
}

ServerNigoriKeyNameChecker::ServerNigoriKeyNameChecker(
    const std::string& expected_key_name,
    syncer::ProfileSyncService* service,
    fake_server::FakeServer* fake_server)
    : SingleClientStatusChangeChecker(service),
      fake_server_(fake_server),
      expected_key_name_(expected_key_name) {}

bool ServerNigoriKeyNameChecker::IsExitConditionSatisfied(std::ostream* os) {
  std::vector<sync_pb::SyncEntity> nigori_entities =
      fake_server_->GetPermanentSyncEntitiesByModelType(syncer::NIGORI);
  DCHECK_EQ(nigori_entities.size(), 1U);

  const std::string given_key_name =
      nigori_entities[0].specifics().nigori().encryption_keybag().key_name();

  *os << "Waiting for a Nigori node with proper key bag encryption key name ("
      << expected_key_name_ << ") to become available on the server."
      << "The server key bag encryption key name is " << given_key_name << ".";
  return given_key_name == expected_key_name_;
}

PassphraseRequiredStateChecker::PassphraseRequiredStateChecker(
    syncer::ProfileSyncService* service,
    bool desired_state)
    : SingleClientStatusChangeChecker(service), desired_state_(desired_state) {}

bool PassphraseRequiredStateChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting until decryption passphrase is " +
             std::string(desired_state_ ? "required" : "not required");
  return service()
             ->GetUserSettings()
             ->IsPassphraseRequiredForPreferredDataTypes() == desired_state_;
}

TrustedVaultKeyRequiredStateChecker::TrustedVaultKeyRequiredStateChecker(
    syncer::ProfileSyncService* service,
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
    syncer::ProfileSyncService* service)
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

ScopedScryptFeatureToggler::ScopedScryptFeatureToggler(
    bool force_disabled,
    bool use_for_new_passphrases) {
  std::vector<base::Feature> enabled_features;
  std::vector<base::Feature> disabled_features;
  if (force_disabled) {
    enabled_features.push_back(
        switches::kSyncForceDisableScryptForCustomPassphrase);
  } else {
    disabled_features.push_back(
        switches::kSyncForceDisableScryptForCustomPassphrase);
  }
  if (use_for_new_passphrases) {
    enabled_features.push_back(switches::kSyncUseScryptForNewCustomPassphrases);
  } else {
    disabled_features.push_back(
        switches::kSyncUseScryptForNewCustomPassphrases);
  }
  feature_list_.InitWithFeatures(enabled_features, disabled_features);
}
