// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/webauthn_credentials_helper.h"

#include <vector>

#include "base/rand_util.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_change.h"
#include "components/webauthn/core/browser/passkey_sync_bridge.h"

namespace webauthn_credentials_helper {

using sync_datatype_helper::test;

namespace {

// Passkey creation timestamps are assumed to increase monotonically to get
// expected behaviour around shadowed credentials. This global tracks the
// "current" timestamp, and is increased on each use.
int g_timestamp = 0;

class WebAuthnCredentialsSyncIdEqualsChecker
    : public MultiClientStatusChangeChecker {
 public:
  WebAuthnCredentialsSyncIdEqualsChecker()
      : MultiClientStatusChangeChecker(test()->GetSyncServices()) {}

  // MultiClientStatusChangeChecker:
  bool IsExitConditionSatisfied(std::ostream* os) override {
    for (int i = 1; i < test()->num_clients(); ++i) {
      if (GetModel(0).GetAllSyncIds() != GetModel(i).GetAllSyncIds()) {
        return false;
      }
    }
    return true;
  }
};

}  // namespace

PasskeySyncActiveChecker::PasskeySyncActiveChecker(
    syncer::SyncServiceImpl* service)
    : SingleClientStatusChangeChecker(service) {}
PasskeySyncActiveChecker::~PasskeySyncActiveChecker() = default;

bool PasskeySyncActiveChecker::IsExitConditionSatisfied(std::ostream* os) {
  return service()->GetActiveDataTypes().Has(syncer::WEBAUTHN_CREDENTIAL);
}

LocalPasskeysChangedChecker::LocalPasskeysChangedChecker(int profile)
    : profile_(profile) {
  observation_.Observe(&GetModel(profile_));
}

LocalPasskeysChangedChecker::~LocalPasskeysChangedChecker() = default;

bool LocalPasskeysChangedChecker::IsExitConditionSatisfied(std::ostream* os) {
  return satisfied_;
}

void LocalPasskeysChangedChecker::OnPasskeysChanged(
    const std::vector<webauthn::PasskeyModelChange>& changes) {
  satisfied_ = true;
  CheckExitCondition();
}

void LocalPasskeysChangedChecker::OnPasskeyModelShuttingDown() {
  observation_.Reset();
}

void LocalPasskeysChangedChecker::OnPasskeyModelIsReady(bool is_ready) {}

LocalPasskeysMatchChecker::LocalPasskeysMatchChecker(int profile,
                                                     Matcher matcher)
    : profile_(profile), matcher_(matcher) {
  observation_.Observe(&GetModel(profile_));
}

LocalPasskeysMatchChecker::~LocalPasskeysMatchChecker() = default;

bool LocalPasskeysMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for local passkeys to match: ";
  testing::StringMatchResultListener result_listener;
  const bool matches = testing::ExplainMatchResult(
      matcher_, GetModel(profile_).GetAllPasskeys(), &result_listener);
  *os << result_listener.str();
  return matches;
}

void LocalPasskeysMatchChecker::OnPasskeysChanged(
    const std::vector<webauthn::PasskeyModelChange>& changes) {
  CheckExitCondition();
}

void LocalPasskeysMatchChecker::OnPasskeyModelShuttingDown() {
  observation_.Reset();
}

void LocalPasskeysMatchChecker::OnPasskeyModelIsReady(bool is_ready) {}

ServerPasskeysMatchChecker::ServerPasskeysMatchChecker(Matcher matcher)
    : matcher_(matcher) {}

ServerPasskeysMatchChecker::~ServerPasskeysMatchChecker() = default;

bool ServerPasskeysMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for server passkeys to match: ";
  std::vector<sync_pb::SyncEntity> entities =
      fake_server()->GetSyncEntitiesByDataType(syncer::WEBAUTHN_CREDENTIAL);
  testing::StringMatchResultListener result_listener;
  const bool matches =
      testing::ExplainMatchResult(matcher_, entities, &result_listener);
  *os << result_listener.str();
  return matches;
}

PasskeyChangeObservationChecker::PasskeyChangeObservationChecker(
    int profile,
    ChangeList expected_changes)
    : profile_(profile), expected_changes_(std::move(expected_changes)) {
  observation_.Observe(&GetModel(profile_));
}

PasskeyChangeObservationChecker::~PasskeyChangeObservationChecker() = default;

bool PasskeyChangeObservationChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting to observe change: ";

  if (expected_changes_.size() != changes_observed_.size()) {
    *os << "Size mismatch: " << expected_changes_.size() << " vs "
        << changes_observed_.size();
    return false;
  }
  for (const auto& change : changes_observed_) {
    if (base::ranges::none_of(
            expected_changes_, [&change](const auto& expected_change) {
              return expected_change.first == change.type() &&
                     expected_change.second == change.passkey().sync_id();
            })) {
      *os << "Unexpected change type " << static_cast<int>(change.type())
          << ", id " << change.passkey().sync_id();
      return false;
    }
  }
  *os << "Match";
  return true;
}

void PasskeyChangeObservationChecker::OnPasskeysChanged(
    const std::vector<webauthn::PasskeyModelChange>& changes) {
  changes_observed_ = changes;
  CheckExitCondition();
}

void PasskeyChangeObservationChecker::OnPasskeyModelShuttingDown() {
  observation_.Reset();
}

void PasskeyChangeObservationChecker::OnPasskeyModelIsReady(bool is_ready) {}

MockPasskeyModelObserver::MockPasskeyModelObserver(
    webauthn::PasskeyModel* model) {
  observation_.Observe(model);
}

MockPasskeyModelObserver::~MockPasskeyModelObserver() = default;

webauthn::PasskeySyncBridge& GetModel(int profile_idx) {
  return *static_cast<webauthn::PasskeySyncBridge*>(
      PasskeyModelFactory::GetForProfile(test()->GetProfile(profile_idx)));
}

bool AwaitAllModelsMatch() {
  return WebAuthnCredentialsSyncIdEqualsChecker().Wait();
}

sync_pb::WebauthnCredentialSpecifics NewPasskey() {
  sync_pb::WebauthnCredentialSpecifics specifics;
  specifics.set_sync_id(base::RandBytesAsString(16));
  specifics.set_credential_id(base::RandBytesAsString(16));
  specifics.set_rp_id(kTestRpId);
  // Pick random user IDs so we don't accidentally create shadow chains. Use
  // `NewShadowingPasskey` to explicitly test shadowing.
  specifics.set_user_id(base::RandBytesAsString(16));
  specifics.set_creation_time(++g_timestamp);
  // Set some random encrypted_data to ensure the model accepts the specifics as
  // valid.
  specifics.set_encrypted("a");
  return specifics;
}

sync_pb::WebauthnCredentialSpecifics NewShadowingPasskey(
    const sync_pb::WebauthnCredentialSpecifics& shadowed) {
  sync_pb::WebauthnCredentialSpecifics specifics;
  specifics.set_sync_id(base::RandBytesAsString(16));
  specifics.set_credential_id(base::RandBytesAsString(16));
  specifics.set_rp_id(shadowed.rp_id());
  specifics.set_user_id(shadowed.user_id());
  specifics.set_creation_time(++g_timestamp);
  specifics.add_newly_shadowed_credential_ids(shadowed.credential_id());
  // Set some random encrypted_data to ensure the model accepts the specifics as
  // valid.
  specifics.set_encrypted("a");
  return specifics;
}

}  // namespace webauthn_credentials_helper
