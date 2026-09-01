// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/ode/passkey_on_device_encryption_state_tracker.h"

#include <string>
#include <tuple>
#include <utility>

#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "chrome/browser/webauthn/mock_enclave_manager.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using ::testing::Bool;
using ::testing::Combine;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Values;

// Used in parameterized tests which verify that the correct state is being
// derived for different combinations of the parameters.
struct StateComputationTestCase {
  bool is_sync_engine_initialized;
  bool is_webauthn_credential_sync_enabled;
  bool is_passkey_model_ready;
  bool is_enclave_loaded;
  bool is_passkey_model_empty;
  bool is_enclave_ready;
  OnDeviceEncryptionState expected_state;
};

class PasskeyOnDeviceEncryptionStateTrackerStateTest
    : public testing::TestWithParam<
          std::tuple<bool /*is_sync_engine_initialized*/,
                     bool /*is_webauthn_credential_sync_enabled*/,
                     bool /*is_passkey_model_ready*/,
                     bool /*is_enclave_loaded*/,
                     bool /*is_passkey_model_empty*/,
                     bool /*is_enclave_ready*/,
                     OnDeviceEncryptionState /*expected_state*/>> {
 public:
  StateComputationTestCase GetTestCase() const {
    return StateComputationTestCase{
        .is_sync_engine_initialized = std::get<0>(GetParam()),
        .is_webauthn_credential_sync_enabled = std::get<1>(GetParam()),
        .is_passkey_model_ready = std::get<2>(GetParam()),
        .is_enclave_loaded = std::get<3>(GetParam()),
        .is_passkey_model_empty = std::get<4>(GetParam()),
        .is_enclave_ready = std::get<5>(GetParam()),
        .expected_state = std::get<6>(GetParam()),
    };
  }

 protected:
  base::test::TaskEnvironment task_environment_;
};

// Used for creating human-readable names for parameterized test cases.
std::string ParamInfoToString(
    const testing::TestParamInfo<
        PasskeyOnDeviceEncryptionStateTrackerStateTest::ParamType>& info) {
  return base::StrCat({
      std::get<0>(info.param) ? "SyncEngineInitialized_"
                              : "SyncEngineNotInitialized_",
      std::get<1>(info.param) ? "WebauthnSyncEnabled_"
                              : "WebauthnSyncDisabled_",
      std::get<2>(info.param) ? "PasskeyModelReady_" : "PasskeyModelNotReady_",
      std::get<3>(info.param) ? "EnclaveLoaded_" : "EnclaveNotLoaded_",
      std::get<4>(info.param) ? "PasskeysEmpty_" : "PasskeysExist_",
      std::get<5>(info.param) ? "EnclaveReady" : "EnclaveNotReady",
  });
}

TEST_P(PasskeyOnDeviceEncryptionStateTrackerStateTest, ComputesCorrectState) {
  const StateComputationTestCase test_case = GetTestCase();

  syncer::TestSyncService sync_service;
  if (!test_case.is_sync_engine_initialized) {
    sync_service.SetMaxTransportState(
        syncer::SyncService::TransportState::INITIALIZING);
  }
  if (test_case.is_webauthn_credential_sync_enabled) {
    sync_service.GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false,
        // `kPasswords` indicates that syncing of passkeys and passwords is
        // active.
        /*types=*/{syncer::UserSelectableType::kPasswords});
  } else {
    sync_service.GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false,
        /*types=*/{});
  }

  webauthn::TestPasskeyModel passkey_model;
  passkey_model.SetReady(test_case.is_passkey_model_ready);
  if (!test_case.is_passkey_model_empty) {
    passkey_model.CreatePasskey(
        "example.com",
        webauthn::PasskeyModel::UserEntity({1, 2, 3}, "user", "User"),
        /*trusted_vault_key=*/{}, /*trusted_vault_key_version=*/0,
        /*public_key_spki_der_out=*/nullptr);
  }

  NiceMock<MockEnclaveManager> enclave_manager;
  ON_CALL(enclave_manager, IsLoaded())
      .WillByDefault(Return(test_case.is_enclave_loaded));
  ON_CALL(enclave_manager, IsReady())
      .WillByDefault(Return(test_case.is_enclave_ready));

  PasskeyOnDeviceEncryptionStateTracker tracker(&sync_service, &enclave_manager,
                                                &passkey_model);
  EXPECT_EQ(tracker.GetEncryptionState(), test_case.expected_state);
}

// When sync engine is not initialized the on-device encryption state can't be
// computed.
INSTANTIATE_TEST_SUITE_P(
    SyncEngineNotInitialized,
    PasskeyOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*is_sync_engine_initialized=*/Values(false),
        /*is_webauthn_credential_sync_enabled=*/Bool(),
        /*is_passkey_model_ready=*/Bool(),
        /*is_enclave_loaded=*/Bool(),
        /*is_passkey_model_empty=*/Bool(),
        /*is_enclave_ready=*/Bool(),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable)),
    &ParamInfoToString);

// Testing the cases when password and passkey sync is disabled.
INSTANTIATE_TEST_SUITE_P(
    WebauthnCredentialSyncNotEnabled,
    PasskeyOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*is_sync_engine_initialized=*/Values(true),
        /*is_webauthn_credential_sync_enabled=*/Values(false),
        /*is_passkey_model_ready=*/Bool(),
        /*is_enclave_loaded=*/Bool(),
        /*is_passkey_model_empty=*/Bool(),
        /*is_enclave_ready=*/Bool(),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kPasswordAndPasskeySyncDisabled)),
    &ParamInfoToString);

// When passkey model is not ready the on-device encryption state can't be
// computed.
INSTANTIATE_TEST_SUITE_P(
    PasskeyModelNotReady,
    PasskeyOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*is_sync_engine_initialized=*/Values(true),
        /*is_webauthn_credential_sync_enabled=*/Values(true),
        /*is_passkey_model_ready=*/Values(false),
        /*is_enclave_loaded=*/Bool(),
        /*is_passkey_model_empty=*/Bool(),
        /*is_enclave_ready=*/Bool(),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable)),
    &ParamInfoToString);

// When enclave manager is not loaded the on-device encryption state can't be
// computed.
INSTANTIATE_TEST_SUITE_P(
    EnclaveManagerNotLoaded,
    PasskeyOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*is_sync_engine_initialized=*/Values(true),
        /*is_webauthn_credential_sync_enabled=*/Values(true),
        /*is_passkey_model_ready=*/Bool(),
        /*is_enclave_loaded=*/Values(false),
        /*is_passkey_model_empty=*/Bool(),
        /*is_enclave_ready=*/Bool(),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable)),
    &ParamInfoToString);

// When passkey model is empty (0 passkeys) we assume that the on-device
// encryption is not enabled.
INSTANTIATE_TEST_SUITE_P(
    PasskeyModelEmpty,
    PasskeyOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*is_sync_engine_initialized=*/Values(true),
        /*is_webauthn_credential_sync_enabled=*/Values(true),
        /*is_passkey_model_ready=*/Values(true),
        /*is_enclave_loaded=*/Values(true),
        /*is_passkey_model_empty=*/Values(true),
        /*is_enclave_ready=*/Bool(),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled)),
    &ParamInfoToString);

// When passkeys exist and enclave is ready the on-device encryption is ready on
// the device.
INSTANTIATE_TEST_SUITE_P(
    DeviceReady,
    PasskeyOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*is_sync_engine_initialized=*/Values(true),
        /*is_webauthn_credential_sync_enabled=*/Values(true),
        /*is_passkey_model_ready=*/Values(true),
        /*is_enclave_loaded=*/Values(true),
        /*is_passkey_model_empty=*/Values(false),
        /*is_enclave_ready=*/Values(true),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kDeviceReady)),
    &ParamInfoToString);

// When passkeys exist but enclave is not ready the on-device encryption is not
// ready on the device.
INSTANTIATE_TEST_SUITE_P(
    DeviceNotReady,
    PasskeyOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*is_sync_engine_initialized=*/Values(true),
        /*is_webauthn_credential_sync_enabled=*/Values(true),
        /*is_passkey_model_ready=*/Values(true),
        /*is_enclave_loaded=*/Values(true),
        /*is_passkey_model_empty=*/Values(false),
        /*is_enclave_ready=*/Values(false),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kDeviceNotReady)),
    &ParamInfoToString);

}  // namespace

}  // namespace password_manager
