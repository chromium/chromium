// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_WEBAUTHN_CREDENTIALS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_WEBAUTHN_CREDENTIALS_HELPER_H_

#include <vector>

#include "base/scoped_observation.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_change.h"
#include "components/webauthn/core/browser/passkey_sync_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_pb {
class SyncEntity;
class WebauthnCredentialSpecifics;
}  // namespace sync_pb

namespace webauthn_credentials_helper {

inline constexpr char kTestRpId[] = "example.com";

// Checker to wait until the WEBAUTHN_CREDENTIAL datatype becomes active.
class PasskeySyncActiveChecker : public SingleClientStatusChangeChecker {
 public:
  explicit PasskeySyncActiveChecker(syncer::SyncServiceImpl* service);
  ~PasskeySyncActiveChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;
};

class LocalPasskeysChangedChecker : public StatusChangeChecker,
                                    public webauthn::PasskeyModel::Observer {
 public:
  explicit LocalPasskeysChangedChecker(int profile);
  ~LocalPasskeysChangedChecker() override;

  // SingleClientStatusChangeChecker:
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // webauthn::PasskeyModel::Observer:
  void OnPasskeysChanged(
      const std::vector<webauthn::PasskeyModelChange>& changes) override;
  void OnPasskeyModelShuttingDown() override;
  void OnPasskeyModelIsReady(bool is_ready) override;

 private:
  int profile_;
  bool satisfied_ = false;
  base::ScopedObservation<webauthn::PasskeyModel,
                          webauthn::PasskeyModel::Observer>
      observation_{this};
};

class LocalPasskeysMatchChecker : public StatusChangeChecker,
                                  public webauthn::PasskeyModel::Observer {
 public:
  using Matcher =
      testing::Matcher<std::vector<sync_pb::WebauthnCredentialSpecifics>>;

  LocalPasskeysMatchChecker(int profile, Matcher matcher);
  ~LocalPasskeysMatchChecker() override;

  // SingleClientStatusChangeChecker:
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // webauthn::PasskeyModel::Observer:
  void OnPasskeysChanged(
      const std::vector<webauthn::PasskeyModelChange>& changes) override;
  void OnPasskeyModelShuttingDown() override;
  void OnPasskeyModelIsReady(bool is_ready) override;

 private:
  const int profile_;
  const Matcher matcher_;
  base::ScopedObservation<webauthn::PasskeyModel,
                          webauthn::PasskeyModel::Observer>
      observation_{this};
};

class ServerPasskeysMatchChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  using Matcher = testing::Matcher<std::vector<sync_pb::SyncEntity>>;

  explicit ServerPasskeysMatchChecker(Matcher matcher);
  ~ServerPasskeysMatchChecker() override;

  // FakeServerMatchStatusChecker:
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const Matcher matcher_;
};

// Observes PasskeyModel changes and waits until the specified list of
// {ChangeType, sync_id} pairs is observed.
class PasskeyChangeObservationChecker
    : public StatusChangeChecker,
      public webauthn::PasskeyModel::Observer {
 public:
  using ChangeList = std::vector<
      std::pair<webauthn::PasskeyModelChange::ChangeType, std::string>>;
  explicit PasskeyChangeObservationChecker(int profile,
                                           ChangeList expected_changes);
  ~PasskeyChangeObservationChecker() override;

  // SingleClientStatusChangeChecker:
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // webauthn::PasskeyModel::Observer:
  void OnPasskeysChanged(
      const std::vector<webauthn::PasskeyModelChange>& changes) override;
  void OnPasskeyModelShuttingDown() override;
  void OnPasskeyModelIsReady(bool is_ready) override;

 private:
  const int profile_;
  std::vector<webauthn::PasskeyModelChange> changes_observed_;
  const ChangeList expected_changes_;
  base::ScopedObservation<webauthn::PasskeyModel,
                          webauthn::PasskeyModel::Observer>
      observation_{this};
};

class MockPasskeyModelObserver : public webauthn::PasskeyModel::Observer {
 public:
  explicit MockPasskeyModelObserver(webauthn::PasskeyModel* model);
  ~MockPasskeyModelObserver() override;

  MOCK_METHOD(void,
              OnPasskeysChanged,
              (const std::vector<webauthn::PasskeyModelChange>&),
              (override));
  MOCK_METHOD(void, OnPasskeyModelShuttingDown, (), (override));
  MOCK_METHOD(void, OnPasskeyModelIsReady, (bool), (override));

 private:
  base::ScopedObservation<webauthn::PasskeyModel,
                          webauthn::PasskeyModel::Observer>
      observation_{this};
};

webauthn::PasskeySyncBridge& GetModel(int profile_idx);

bool AwaitAllModelsMatch();

// Returns a new WebauthnCredentialSpecifics entity with a random sync ID,
// credential ID and user ID, and a fixed RP ID.
sync_pb::WebauthnCredentialSpecifics NewPasskey();

// Returns a new WebauthnCredentialSpecifics entity shadowing another one. Sync
// ID and credential ID are random, while user ID and RP ID will match the other
// credential.
sync_pb::WebauthnCredentialSpecifics NewShadowingPasskey(
    const sync_pb::WebauthnCredentialSpecifics& shadowed);

// Tests that a `sync_pb::SyncEntity` has WebauthnCredentialSpecifics with the
// given `sync_id`. Use with `ServerPasskeysMatchChecker`.
MATCHER_P(EntityHasSyncId, expected_sync_id, "") {
  return arg.specifics().webauthn_credential().sync_id() == expected_sync_id;
}

MATCHER_P(EntityHasUsername, expected_username, "") {
  return arg.specifics().webauthn_credential().user_name() == expected_username;
}

MATCHER_P(EntityHasDisplayName, expected_display_name, "") {
  return arg.specifics().webauthn_credential().user_display_name() ==
         expected_display_name;
}

MATCHER_P(EntityHasLastUsedTime, expected_last_used_time, "") {
  return arg.specifics()
             .webauthn_credential()
             .last_used_time_windows_epoch_micros() == expected_last_used_time;
}

// Matches a `sync_pb::WebauthnCredentialSpecifics` against another field by
// field.
MATCHER_P(PasskeySpecificsEq, expected, "") {
  return arg.sync_id() == expected.sync_id() &&
         arg.credential_id() == expected.credential_id() &&
         arg.rp_id() == expected.rp_id() &&
         base::ranges::equal(arg.newly_shadowed_credential_ids().begin(),
                             arg.newly_shadowed_credential_ids().end(),
                             expected.newly_shadowed_credential_ids().begin(),
                             expected.newly_shadowed_credential_ids().end()) &&
         arg.creation_time() == expected.creation_time() &&
         arg.user_name() == expected.user_name() &&
         arg.user_display_name() == expected.user_display_name() &&
         arg.third_party_payments_support() ==
             expected.third_party_payments_support() &&
         arg.last_used_time_windows_epoch_micros() ==
             expected.last_used_time_windows_epoch_micros() &&
         arg.key_version() == expected.key_version() &&
         arg.has_private_key() == expected.has_private_key() &&
         arg.private_key() == expected.private_key() &&
         arg.has_encrypted() == expected.has_encrypted() &&
         arg.encrypted() == expected.encrypted();
}

// Matches the `sync_id` of a `sync_pb::WebauthnCredentialSpecifics`. Use with
// `LocalPasskeysMatchChecker`.
MATCHER_P(PasskeyHasSyncId, expected_sync_id, "") {
  return arg.sync_id() == expected_sync_id;
}

// Matches the `rp_id` of a `sync_pb::WebauthnCredentialSpecifics`. Use with
// `LocalPasskeysMatchChecker`.
MATCHER_P(PasskeyHasRpId, expected_rp_id, "") {
  return arg.rp_id() == expected_rp_id;
}

// Matches the `user_id` of a `sync_pb::WebauthnCredentialSpecifics`. Use with
// `LocalPasskeysMatchChecker`.
MATCHER_P(PasskeyHasUserId, expected_user_id, "") {
  return arg.user_id() == expected_user_id;
}

// Matches the `display_name` of a `sync_pb::WebauthnCredentialSpecifics`. Use
// with `LocalPasskeysMatchChecker`.
MATCHER_P(PasskeyHasDisplayName, expected_display_name, "") {
  return arg.user_display_name() == expected_display_name;
}

}  // namespace webauthn_credentials_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_WEBAUTHN_CREDENTIALS_HELPER_H_
