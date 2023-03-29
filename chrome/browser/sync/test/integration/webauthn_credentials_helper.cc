// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/webauthn_credentials_helper.h"

#include "base/rand_util.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_model.h"

namespace webauthn_credentials_helper {

using sync_datatype_helper::test;

namespace {

constexpr char kTestRpId[] = "example.com";
constexpr char kTestUserId[] = "\x01\x02\x03";

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

PasskeyModel& GetModel(int profile_idx) {
  return *PasskeyModelFactory::GetForProfile(test()->GetProfile(profile_idx));
}

bool AwaitAllModelsMatch() {
  return WebAuthnCredentialsSyncIdEqualsChecker().Wait();
}

sync_pb::WebauthnCredentialSpecifics NewPasskey() {
  sync_pb::WebauthnCredentialSpecifics specifics;
  specifics.set_credential_id(base::RandBytesAsString(16));
  specifics.set_rp_id(kTestRpId);
  specifics.set_user_id(kTestUserId);
  return specifics;
}

}  // namespace webauthn_credentials_helper
