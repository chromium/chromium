// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/ode/password_trusted_vault_on_device_encryption_state_tracker.h"

#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/containers/enum_set.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using ::testing::Bool;
using ::testing::Combine;
using ::testing::Values;
using ::testing::ValuesIn;

constexpr auto AllPassphraseTypeSet =
    base::EnumSet<syncer::PassphraseType, syncer::PassphraseType{0}>::All();
constexpr auto NonTrustedVaultPassphraseTypeSet =
    base::Difference(AllPassphraseTypeSet,
                     decltype(AllPassphraseTypeSet){
                         syncer::PassphraseType::kTrustedVaultPassphrase});

// Used in parameterized tests which verify that the correct state is being
// derived for different combinations of the parameters.
struct StateComputationTestCase {
  bool is_sync_engine_initialized;
  bool is_password_sync_enabled;
  syncer::PassphraseType passphrase_type;
  bool is_trusted_vault_key_required;
  bool is_passwords_data_type_active;
  OnDeviceEncryptionState expected_state;
};

class PasswordTrustedVaultOnDeviceEncryptionStateTrackerStateTest
    : public testing::TestWithParam<
          std::tuple<bool /*is_sync_engine_initialized*/,
                     bool /*is_password_sync_enabled*/,
                     syncer::PassphraseType /*passphrase_type*/,
                     bool /*is_trusted_vault_key_required*/,
                     bool /*is_passwords_data_type_active*/,
                     OnDeviceEncryptionState /*expected_state*/>> {
 public:
  StateComputationTestCase GetTestCase() const {
    return StateComputationTestCase{
        .is_sync_engine_initialized = std::get<0>(GetParam()),
        .is_password_sync_enabled = std::get<1>(GetParam()),
        .passphrase_type = std::get<2>(GetParam()),
        .is_trusted_vault_key_required = std::get<3>(GetParam()),
        .is_passwords_data_type_active = std::get<4>(GetParam()),
        .expected_state = std::get<5>(GetParam()),
    };
  }

 protected:
  base::test::TaskEnvironment task_environment_;
};

std::string_view PassphraseTypeToString(syncer::PassphraseType type) {
  switch (type) {
    case syncer::PassphraseType::kImplicitPassphrase:
      return "ImplicitPassphrase";
    case syncer::PassphraseType::kKeystorePassphrase:
      return "KeystorePassphrase";
    case syncer::PassphraseType::kFrozenImplicitPassphrase:
      return "FrozenImplicitPassphrase";
    case syncer::PassphraseType::kCustomPassphrase:
      return "CustomPassphrase";
    case syncer::PassphraseType::kTrustedVaultPassphrase:
      return "TrustedVaultPassphrase";
  }
}

// Used for creating human-readable names for parameterized test cases.
std::string ParamInfoToString(
    const testing::TestParamInfo<
        PasswordTrustedVaultOnDeviceEncryptionStateTrackerStateTest::ParamType>&
        info) {
  return base::StrCat({
      std::get<0>(info.param) ? "SyncEngineInitialized_"
                              : "SyncEngineNotInitialized_",
      std::get<1>(info.param) ? "PasswordSyncEnabled_"
                              : "PasswordSyncDisabled_",
      PassphraseTypeToString(std::get<2>(info.param)),
      "_",
      std::get<3>(info.param) ? "KeyRequired_" : "KeyNotRequired_",
      std::get<4>(info.param) ? "PasswordsActive" : "PasswordsNotActive",
  });
}

TEST_P(PasswordTrustedVaultOnDeviceEncryptionStateTrackerStateTest,
       ComputesCorrectState) {
  const StateComputationTestCase test_case = GetTestCase();

  syncer::TestSyncService sync_service;
  if (!test_case.is_sync_engine_initialized) {
    sync_service.SetMaxTransportState(
        syncer::SyncService::TransportState::INITIALIZING);
  }
  if (test_case.is_password_sync_enabled) {
    sync_service.GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false,
        /*types=*/{syncer::UserSelectableType::kPasswords});
  } else {
    sync_service.GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false,
        /*types=*/{});
  }
  sync_service.GetUserSettings()->SetPassphraseType(test_case.passphrase_type);
  sync_service.GetUserSettings()->SetTrustedVaultKeyRequired(
      test_case.is_trusted_vault_key_required);
  if (!test_case.is_passwords_data_type_active) {
    sync_service.SetFailedDataTypes({syncer::PASSWORDS});
  }

  PasswordTrustedVaultOnDeviceEncryptionStateTracker tracker(&sync_service);
  EXPECT_EQ(tracker.GetEncryptionState(), test_case.expected_state);
}

// When sync engine is not initialized the on-device encryption state can't be
// computed.
INSTANTIATE_TEST_SUITE_P(
    SyncEngineNotInitialized,
    PasswordTrustedVaultOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*is_sync_engine_initialized=*/Values(false),
        /*is_password_sync_enabled=*/Bool(),
        /*passphrase_type=*/ValuesIn(AllPassphraseTypeSet),
        /*is_trusted_vault_key_required=*/Bool(),
        /*is_passwords_data_type_active=*/Bool(),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable)),
    &ParamInfoToString);

// Testing the cases when password and passkey sync is disabled.
INSTANTIATE_TEST_SUITE_P(
    PasswordSyncNotEnabled,
    PasswordTrustedVaultOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*is_sync_engine_initialized=*/Values(true),
        /*is_password_sync_enabled=*/Values(false),
        /*passphrase_type=*/ValuesIn(AllPassphraseTypeSet),
        /*is_trusted_vault_key_required=*/Bool(),
        /*is_passwords_data_type_active=*/Bool(),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kPasswordAndPasskeySyncDisabled)),
    &ParamInfoToString);

// When passphrase type is not trusted vault the on-device encryption state is
// "not enabled".
INSTANTIATE_TEST_SUITE_P(
    PassphraseTypeNotTrustedVault,
    PasswordTrustedVaultOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*is_sync_engine_initialized=*/Values(true),
        /*is_password_sync_enabled=*/Values(true),
        /*passphrase_type=*/ValuesIn(NonTrustedVaultPassphraseTypeSet),
        /*is_trusted_vault_key_required=*/Bool(),
        /*is_passwords_data_type_active=*/Bool(),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled)),
    &ParamInfoToString);

// When trusted vault key is not required but passwords data type is not active
// yet (asynchronous check for local keys is in flight), the on-device
// encryption state can't be computed.
INSTANTIATE_TEST_SUITE_P(
    KeyCheckInFlight,
    PasswordTrustedVaultOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*is_sync_engine_initialized=*/Values(true),
        /*is_password_sync_enabled=*/Values(true),
        /*passphrase_type=*/
        Values(syncer::PassphraseType::kTrustedVaultPassphrase),
        /*is_trusted_vault_key_required=*/Values(false),
        /*is_passwords_data_type_active=*/Values(false),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable)),
    &ParamInfoToString);

// When trusted vault key is required the on-device encryption is not ready on
// the device.
INSTANTIATE_TEST_SUITE_P(
    DeviceNotReady,
    PasswordTrustedVaultOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*is_sync_engine_initialized=*/Values(true),
        /*is_password_sync_enabled=*/Values(true),
        /*passphrase_type=*/
        Values(syncer::PassphraseType::kTrustedVaultPassphrase),
        /*is_trusted_vault_key_required=*/Values(true),
        /*is_passwords_data_type_active=*/Bool(),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kDeviceNotReady)),
    &ParamInfoToString);

// When trusted vault key is not required and passwords data type is active the
// on-device encryption is ready on the device.
INSTANTIATE_TEST_SUITE_P(
    DeviceReady,
    PasswordTrustedVaultOnDeviceEncryptionStateTrackerStateTest,
    Combine(
        /*is_sync_engine_initialized=*/Values(true),
        /*is_password_sync_enabled=*/Values(true),
        /*passphrase_type=*/
        Values(syncer::PassphraseType::kTrustedVaultPassphrase),
        /*is_trusted_vault_key_required=*/Values(false),
        /*is_passwords_data_type_active=*/Values(true),
        /*expected_state=*/
        Values(OnDeviceEncryptionState::kDeviceReady)),
    &ParamInfoToString);

}  // namespace

}  // namespace password_manager
