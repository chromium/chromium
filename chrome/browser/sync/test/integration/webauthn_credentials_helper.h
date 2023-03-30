// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_WEBAUTHN_CREDENTIALS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_WEBAUTHN_CREDENTIALS_HELPER_H_

#include <vector>

#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_pb {
class SyncEntity;
class WebauthnCredentialSpecifics;
}

namespace webauthn_credentials_helper {

class LocalPasskeysMatchChecker : public SingleClientStatusChangeChecker {
 public:
  using Matcher =
      testing::Matcher<std::vector<sync_pb::WebauthnCredentialSpecifics>>;

  LocalPasskeysMatchChecker(int profile, Matcher matcher);
  ~LocalPasskeysMatchChecker() override;

  // SingleClientStatusChangeChecker:
  bool IsExitConditionSatisfied(std::ostream* os) override;
  void OnSyncCycleCompleted(syncer::SyncService* sync) override;

 private:
  const int profile_;
  const Matcher matcher_;
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

PasskeyModel& GetModel(int profile_idx);

bool AwaitAllModelsMatch();

// Returns a new WebauthnCredentialSpecifics entity with a random sync ID and
// credential ID, and fixed RP ID and user ID.
sync_pb::WebauthnCredentialSpecifics NewPasskey();

// Tests that a `sync_pb::SyncEntity` has WebauthnCredentialSpecifics with the
// given `sync_id`. Use with `ServerPasskeysMatchChecker`.
MATCHER_P(EntityHasSyncId, expected_sync_id, "") {
  return arg.specifics().webauthn_credential().sync_id() == expected_sync_id;
}

// Matches the `sync_id` of a `sync_pb::WebauthnCredentialSpecifics`. Use with
// `LocalPasskeysMatchChecker`.
MATCHER_P(PasskeyHasSyncId, expected_sync_id, "") {
  return arg.sync_id() == expected_sync_id;
}

}  // namespace webauthn_credentials_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_WEBAUTHN_CREDENTIALS_HELPER_H_
