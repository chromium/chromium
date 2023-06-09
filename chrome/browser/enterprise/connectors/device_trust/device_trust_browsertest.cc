// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/navigation_throttle.h"
#include "chrome/browser/enterprise/connectors/device_trust/test/device_trust_browsertest_base.h"
#include "chrome/browser/enterprise/connectors/device_trust/test/test_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/device_signals/test/signals_contract.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/device_trust/test/device_trust_test_environment_win.h"
#include "chrome/browser/enterprise/connectors/test/test_constants.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#endif  // #if BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/attestation/mock_tpm_challenge_key.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_result.h"
#else
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/scoped_key_rotation_command_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#endif

using content::NavigationHandle;

namespace enterprise_connectors::test {

namespace {

constexpr char kChallengeV1[] =
    "{\"challenge\": "
    "{"
    "\"data\": "
    "\"ChZFbnRlcnByaXNlS2V5Q2hhbGxlbmdlEiABAZTXEb/mB+E3Ncja9cazVIg3frBMjxpc"
    "UfyWoC+M6xjOmrvJ0y8=\","
    "\"signature\": "
    "\"cEA1rPdSEuBaM/4cWOv8R/OicR5c8IT+anVnVd7ain6ucZuyyy/8sjWYK4JpvVu2Diy6y"
    "6a77/5mis+QRNsbjVQ1QkEf7TcQOaGitt618jwQyhc54cyGhKUiuCok8Q7jc2gwrN6POKmB"
    "3Vdx+nrhmmVjzp/QAGgamPoLQmuW5XM+Cq5hSrW/U8bg12KmrZ5OHYdiZLyGGlmgE811kpxq"
    "dKQSWWB1c2xiu5ALY0q8aa8o/Hrzqko8JJbMXcefwrr9YxcEAoVH524mjtj83Pru55WfPmDL"
    "2ZgSJhErFEQDvWjyX0cDuFX8fO2i40aAwJsFoX+Z5fHbd3kanTcK+ty56w==\""
    "}"
    "}";

#if BUILDFLAG(IS_WIN)
constexpr char kFakeNonce[] = "fake nonce";
constexpr int kSuccessCode = 200;
constexpr int kHardFailureCode = 400;
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
DeviceTrustConnectorState CreateManagedDeviceState() {
  DeviceTrustConnectorState state;

  state.cloud_machine_management_level.is_managed = true;

  // In case user management is added.
  state.affiliated = true;

  return state;
}
#else
DeviceTrustConnectorState CreateUnmanagedState() {
  return DeviceTrustConnectorState();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

#if BUILDFLAG(IS_CHROMEOS_ASH)
class DeviceTrustAshBrowserTest : public test::DeviceTrustBrowserTestBase {
 protected:
  explicit DeviceTrustAshBrowserTest(
      absl::optional<DeviceTrustConnectorState> state = absl::nullopt)
      : DeviceTrustBrowserTestBase(std::move(state)) {
    auto mock_challenge_key =
        std::make_unique<ash::attestation::MockTpmChallengeKey>();
    mock_challenge_key->EnableFake();
    ash::attestation::TpmChallengeKeyFactory::SetForTesting(
        std::move(mock_challenge_key));
  }

  void TearDownOnMainThread() override {
    ash::attestation::TpmChallengeKeyFactory::Create();
    test::DeviceTrustBrowserTestBase::TearDownOnMainThread();
  }
};

using DeviceTrustBrowserTest = DeviceTrustAshBrowserTest;
#else
class DeviceTrustDesktopBrowserTest : public test::DeviceTrustBrowserTestBase {
 protected:
  explicit DeviceTrustDesktopBrowserTest(
      absl::optional<DeviceTrustConnectorState> state)
      : DeviceTrustDesktopBrowserTest(true, std::move(state)) {}

  explicit DeviceTrustDesktopBrowserTest(
      bool create_preexisting_key = true,
      absl::optional<DeviceTrustConnectorState> state = absl::nullopt)
      : DeviceTrustBrowserTestBase(std::move(state)),
        create_preexisting_key_(create_preexisting_key) {}

  void SetUpInProcessBrowserTestFixture() override {
    test::DeviceTrustBrowserTestBase::SetUpInProcessBrowserTestFixture();
#if BUILDFLAG(IS_WIN)
    device_trust_test_environment_win_.emplace();
    device_trust_test_environment_win_->SetExpectedDMToken(kBrowserDmToken);
    device_trust_test_environment_win_->SetExpectedClientID(kBrowserClientId);

    if (create_preexisting_key_) {
      device_trust_test_environment_win_->SetUpExistingKey();
    }
#else  // BUILDFLAG(IS_WIN)
    scoped_persistence_delegate_factory_.emplace();
    scoped_rotation_command_factory_.emplace();
#endif
  }

  // If set to true, will fake as if a key was already persisted on the device
  // before the browser starts.
  const bool create_preexisting_key_;

#if BUILDFLAG(IS_WIN)
  absl::optional<DeviceTrustTestEnvironmentWin>
      device_trust_test_environment_win_;
#else  // BUILDFLAG(IS_WIN)
  absl::optional<ScopedKeyPersistenceDelegateFactory>
      scoped_persistence_delegate_factory_;
  absl::optional<ScopedKeyRotationCommandFactory>
      scoped_rotation_command_factory_;
#endif
};

using DeviceTrustBrowserTest = DeviceTrustDesktopBrowserTest;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Tests that the whole attestation flow occurs when navigating to an
// allowed domain.
IN_PROC_BROWSER_TEST_F(DeviceTrustBrowserTest, AttestationFullFlowKeyExists) {
  TriggerUrlNavigation();
  VerifyAttestationFlowSuccessful();
}

IN_PROC_BROWSER_TEST_F(DeviceTrustBrowserTest, AttestationFullFlowKeyExistsV1) {
  SetChallengeValue(kChallengeV1);
  TriggerUrlNavigation();
  VerifyAttestationFlowFailure(test::kFailedToParseChallengeJsonResponse);
}

class DeviceTrustDisabledBrowserTest : public DeviceTrustBrowserTest {
 protected:
  DeviceTrustDisabledBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(kDeviceTrustConnectorEnabled,
                                              false);
  }
};

IN_PROC_BROWSER_TEST_F(DeviceTrustDisabledBrowserTest,
                       AttestationFullFlowKeyExists) {
  TriggerUrlNavigation();
  VerifyNoInlineFlowOccurred();
}

// Tests that the attestation flow does not get triggered when navigating to a
// domain that is not part of the allow-list.
IN_PROC_BROWSER_TEST_F(DeviceTrustBrowserTest, AttestationHostNotAllowed) {
  TriggerUrlNavigation(GetDisallowedUrl());

  // Requests with attestation flow headers should not have been recorded.
  VerifyNoInlineFlowOccurred();
}

// Tests that the attestation flow does not get triggered when the allow-list is
// empty.
IN_PROC_BROWSER_TEST_F(DeviceTrustBrowserTest, AttestationPrefEmptyList) {
  // Clear the allow-list Pref and trigger a navigation.
  device_trust_mixin_->DisableAllInlinePolicies();
  TriggerUrlNavigation();

  // Requests with attestation flow headers should not have been recorded.
  VerifyNoInlineFlowOccurred();
}

// Tests that the device trust navigation throttle does not get created for a
// navigation handle in incognito mode.
IN_PROC_BROWSER_TEST_F(DeviceTrustBrowserTest,
                       CreateNavigationThrottleIncognitoMode) {
  // Add incognito browser for the mock navigation handle.
  auto* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  content::MockNavigationHandle mock_nav_handle(
      web_contents(incognito_browser));

  // Try to create the device trust navigation throttle.
  EXPECT_FALSE(enterprise_connectors::DeviceTrustNavigationThrottle::
                   MaybeCreateThrottleFor(&mock_nav_handle));
}

class DeviceTrustDelayedManagementBrowserTest
    : public DeviceTrustBrowserTest,
      public ::testing::WithParamInterface<DeviceTrustConnectorState> {
 protected:
  DeviceTrustDelayedManagementBrowserTest()
      : DeviceTrustBrowserTest(GetParam()) {
    scoped_feature_list_.InitWithFeatureState(kUserDTCInlineFlowEnabled, true);
  }
};

// Tests that the device trust navigation throttle does not get created when
// there is no user management and later gets created when user management is
// added to the same context, unless the feature flag is disabled.
IN_PROC_BROWSER_TEST_P(DeviceTrustDelayedManagementBrowserTest,
                       ManagementAddedAfterFirstCreationTry) {
  content::MockNavigationHandle mock_nav_handle(web_contents());

  TriggerUrlNavigation();
  VerifyNoInlineFlowOccurred();

  // Profile user becomes managed.
  device_trust_mixin_->ManageCloudUser();

  ResetState();
  TriggerUrlNavigation();
  VerifyNoInlineFlowOccurred();

  // DTC policy is enabled for that user.
  device_trust_mixin_->EnableUserInlinePolicy();

  DTAttestationResult success_result = DTAttestationResult::kSuccess;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // On desktop platforms, consent is required when the device is not managed.
  device_trust_mixin_->SetConsentGiven(true);

  // Also, attestation is not yet supported.
  success_result = DTAttestationResult::kSuccessNoSignature;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  ResetState();
  TriggerUrlNavigation();
  VerifyAttestationFlowSuccessful(success_result);
}

INSTANTIATE_TEST_SUITE_P(,
                         DeviceTrustDelayedManagementBrowserTest,
                         testing::Values(
#if BUILDFLAG(IS_CHROMEOS_ASH)
                             CreateManagedDeviceState()
#else
                             CreateUnmanagedState()
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
                                 ));

// Tests that signal values respect the expected format and is filled-out as
// expect per platform.
IN_PROC_BROWSER_TEST_F(DeviceTrustBrowserTest, SignalsContract) {
  auto* device_trust_service =
      DeviceTrustServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(device_trust_service);

  base::test::TestFuture<base::Value::Dict> future;
  device_trust_service->GetSignals(future.GetCallback());

  // This error most likely indicates that one of the signals decorators did
  // not invoke its done_closure in time.
  ASSERT_TRUE(future.Wait()) << "Timed out while collecting signals.";

  const base::Value::Dict& signals_dict = future.Get();

  const auto signals_contract_map = device_signals::test::GetSignalsContract();
  ASSERT_FALSE(signals_contract_map.empty());
  for (const auto& signals_contract_entry : signals_contract_map) {
    // First is the signal name.
    // Second is the contract evaluation predicate.
    EXPECT_TRUE(signals_contract_entry.second.Run(signals_dict))
        << "Signals contract validation failed for: "
        << signals_contract_entry.first;
  }
}

#if BUILDFLAG(IS_WIN)

using KeyRotationResult = DeviceTrustKeyManager::KeyRotationResult;

// To test "create key" flows, there should be no pre-existing persisted key.
class DeviceTrustCreateKeyBrowserTest : public DeviceTrustDesktopBrowserTest {
 protected:
  DeviceTrustCreateKeyBrowserTest()
      : DeviceTrustDesktopBrowserTest(/*create_preexisting_key=*/false) {}
};

// Windows DT test environment mocks the registry and DT key does not exist by
// default, in this test case a key will be created by DeviceTrustKeyManager.
IN_PROC_BROWSER_TEST_F(DeviceTrustCreateKeyBrowserTest,
                       AttestationFullFlowKeyCreation) {
  TriggerUrlNavigation();
  VerifyAttestationFlowSuccessful();
}

IN_PROC_BROWSER_TEST_F(DeviceTrustCreateKeyBrowserTest,
                       AttestationFullFlowKeyCreationV1) {
  SetChallengeValue(kChallengeV1);
  TriggerUrlNavigation();
  VerifyAttestationFlowFailure(test::kFailedToParseChallengeJsonResponse);
}

// To test "create key" flows where the initial upload fails, the response code
// needs to be mocked before the browser starts.
class DeviceTrustCreateKeyUploadFailedBrowserTest
    : public DeviceTrustCreateKeyBrowserTest {
 protected:
  DeviceTrustCreateKeyUploadFailedBrowserTest()
      : DeviceTrustCreateKeyBrowserTest() {}

  void SetUpOnMainThread() override {
    DeviceTrustCreateKeyBrowserTest::SetUpOnMainThread();

    // First attestation flow attempt fails when a DT attestation key does not
    // exist, and KeyRotationCommand fails to upload the newly created key.
    device_trust_test_environment_win_->SetUploadResult(kHardFailureCode);
  }
};

IN_PROC_BROWSER_TEST_F(DeviceTrustCreateKeyUploadFailedBrowserTest,
                       AttestationFullFlowSucceedOnThirdAttempt) {
  TriggerUrlNavigation();
  VerifyAttestationFlowSuccessful(DTAttestationResult::kSuccessNoSignature);
  // DT attestation key should not be created if attestation fails.
  ASSERT_FALSE(device_trust_test_environment_win_->KeyExists());

  // Second attestation flow attempt fails when key upload fails again, this is
  // for testing that consecutive failures does not break anything
  ResetState();
  TriggerUrlNavigation();
  VerifyAttestationFlowSuccessful(DTAttestationResult::kSuccessNoSignature);
  ASSERT_FALSE(device_trust_test_environment_win_->KeyExists());

  // Third attestation flow attempt succeeds after two failed attempts, this is
  // for testing that previous failed attempts does not affect new attempts from
  // succeeding AND that metrics is working at the same time.
  device_trust_test_environment_win_->SetUploadResult(kSuccessCode);
  ResetState();
  TriggerUrlNavigation();
  VerifyAttestationFlowSuccessful();
  ASSERT_TRUE(device_trust_test_environment_win_->KeyExists());
}

IN_PROC_BROWSER_TEST_F(DeviceTrustDesktopBrowserTest,
                       RemoteCommandKeyRotationSuccess) {
  // Make sure the key is present and store its current value.
  std::vector<uint8_t> current_key_pair =
      device_trust_test_environment_win_->GetWrappedKey();
  ASSERT_FALSE(current_key_pair.empty());

  auto* key_manager = g_browser_process->browser_policy_connector()
                          ->chrome_browser_cloud_management_controller()
                          ->GetDeviceTrustKeyManager();

  base::test::TestFuture<KeyRotationResult> future_result;
  key_manager->RotateKey(kFakeNonce, future_result.GetCallback());
  ASSERT_EQ(future_result.Get(), KeyRotationResult::SUCCESS);

  // Check that key still exists & is replaced with new value.
  ASSERT_TRUE(device_trust_test_environment_win_->KeyExists());
  EXPECT_NE(device_trust_test_environment_win_->GetWrappedKey(),
            current_key_pair);
}

IN_PROC_BROWSER_TEST_F(DeviceTrustDesktopBrowserTest,
                       RemoteCommandKeyRotationFailure) {
  // Make sure key presents and stores its current value.
  std::vector<uint8_t> current_key_pair =
      device_trust_test_environment_win_->GetWrappedKey();
  ASSERT_FALSE(current_key_pair.empty());

  // Force key upload to fail, in turn failing the key rotation
  device_trust_test_environment_win_->SetUploadResult(kHardFailureCode);

  auto* key_manager = g_browser_process->browser_policy_connector()
                          ->chrome_browser_cloud_management_controller()
                          ->GetDeviceTrustKeyManager();

  base::test::TestFuture<KeyRotationResult> future_result;
  key_manager->RotateKey(kFakeNonce, future_result.GetCallback());
  ASSERT_EQ(future_result.Get(), KeyRotationResult::FAILURE);

  // Check that key still exists & has the same value since rotation failed.
  ASSERT_TRUE(device_trust_test_environment_win_->KeyExists());
  EXPECT_EQ(device_trust_test_environment_win_->GetWrappedKey(),
            current_key_pair);
}

class DeviceTrustDisabledCreateKeyBrowserTest
    : public DeviceTrustCreateKeyBrowserTest {
 protected:
  DeviceTrustDisabledCreateKeyBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(kDeviceTrustConnectorEnabled,
                                              false);
  }
};
IN_PROC_BROWSER_TEST_F(DeviceTrustDisabledCreateKeyBrowserTest,
                       AttestationFullFlowKeyCreation) {
  TriggerUrlNavigation();
  VerifyNoInlineFlowOccurred();
  ASSERT_FALSE(device_trust_test_environment_win_->KeyExists());
}

#endif

}  // namespace enterprise_connectors::test
