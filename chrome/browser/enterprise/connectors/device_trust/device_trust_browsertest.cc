// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/metrics_utils.h"
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
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/device_trust/test/device_trust_test_environment_win.h"
#include "chrome/browser/enterprise/connectors/test/test_constants.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#endif  // #if BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/attestation/mock_tpm_challenge_key.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_result.h"
#else
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/scoped_key_rotation_command_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/device_signals/core/browser/pref_names.h"
#include "components/device_signals/core/common/signals_features.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#include "ui/base/interaction/element_identifier.h"
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

DeviceTrustConnectorState CreateUnmanagedState() {
  return DeviceTrustConnectorState();
}

}  // namespace

#if BUILDFLAG(IS_CHROMEOS_ASH)
class DeviceTrustAshBrowserTest : public test::DeviceTrustBrowserTestBase {
 protected:
  explicit DeviceTrustAshBrowserTest(
      std::optional<DeviceTrustConnectorState> state = std::nullopt)
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
      std::optional<DeviceTrustConnectorState> state)
      : DeviceTrustDesktopBrowserTest(true, std::move(state)) {}

  explicit DeviceTrustDesktopBrowserTest(
      bool create_preexisting_key = true,
      std::optional<DeviceTrustConnectorState> state = std::nullopt)
      : DeviceTrustBrowserTestBase(std::move(state)),
        create_preexisting_key_(create_preexisting_key) {}

  void SetUpInProcessBrowserTestFixture() override {
    test::DeviceTrustBrowserTestBase::SetUpInProcessBrowserTestFixture();
#if BUILDFLAG(IS_WIN)
    device_trust_test_environment_win_.emplace();
    device_trust_test_environment_win_->SetExpectedDMToken(kBrowserDmToken);
    device_trust_test_environment_win_->SetExpectedClientID(kBrowserClientId);

    // This will set up a key before DeviceTrustKeyManager initializes.
    // DTKM should just try to load this key instead of creating one itself.
    // If create_preexisting_key_ is False, then DTKM is responsible for
    // creating the key and put it in storage.
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
  std::optional<DeviceTrustTestEnvironmentWin>
      device_trust_test_environment_win_;
#else  // BUILDFLAG(IS_WIN)
  std::optional<ScopedKeyPersistenceDelegateFactory>
      scoped_persistence_delegate_factory_;
  std::optional<ScopedKeyRotationCommandFactory>
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
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
#if BUILDFLAG(IS_MAC)
            kDTCKeyRotationUploadedBySharedAPIEnabled,
#endif  // BUILDFLAG(IS_MAC)
            kDTCKeyUploadedBySharedAPIEnabled,
#if BUILDFLAG(IS_CHROMEOS_ASH)
            ash::features::kUnmanagedDeviceDeviceTrustConnectorEnabled
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
        },
        /*disabled_features=*/{});
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

INSTANTIATE_TEST_SUITE_P(UnmanagedState,
                         DeviceTrustDelayedManagementBrowserTest,
                         testing::Values(CreateUnmanagedState()));

#if BUILDFLAG(IS_CHROMEOS_ASH)
INSTANTIATE_TEST_SUITE_P(ManagedState,
                         DeviceTrustDelayedManagementBrowserTest,
                         testing::Values(CreateManagedDeviceState()));

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
// Setting create_preexisting_key to false will result in no key existing
// when DeviceTrustKeyManager initializes, and it should create a key
// in storage.
class DeviceTrustCreateKeyBrowserTest : public DeviceTrustDesktopBrowserTest {
 protected:
  DeviceTrustCreateKeyBrowserTest()
      : DeviceTrustDesktopBrowserTest(/*create_preexisting_key=*/false) {}
};

IN_PROC_BROWSER_TEST_F(DeviceTrustCreateKeyBrowserTest,
                       AttestationFullFlowKeyCreation) {
  TriggerUrlNavigation();
  VerifyAttestationFlowSuccessful();
  // Make sure DeviceTrustKeyManager successfully created a key in storage
  // via no-nonce key rotation.
  VerifyKeyRotationSuccess(/*with_nonce=*/false);

  EXPECT_FALSE(device_trust_test_environment_win_->GetWrappedKey().empty());
}

IN_PROC_BROWSER_TEST_F(DeviceTrustCreateKeyBrowserTest,
                       AttestationFullFlowKeyCreationV1) {
  SetChallengeValue(kChallengeV1);
  TriggerUrlNavigation();
  VerifyAttestationFlowFailure(test::kFailedToParseChallengeJsonResponse);
  VerifyKeyRotationSuccess(/*with_nonce=*/false);

  EXPECT_FALSE(device_trust_test_environment_win_->GetWrappedKey().empty());
}

// To test "create key" flows where the initial upload fails, the response code
// needs to be mocked before the browser starts.
class DeviceTrustCreateKeyUploadFailedBrowserTest
    : public DeviceTrustCreateKeyBrowserTest {
 protected:
  DeviceTrustCreateKeyUploadFailedBrowserTest()
      : DeviceTrustCreateKeyBrowserTest() {}
  void SetUpInProcessBrowserTestFixture() override {
    DeviceTrustCreateKeyBrowserTest::SetUpInProcessBrowserTestFixture();
    device_trust_test_environment_win_->SetUploadResult(kHardFailureCode);
  }
};

// TODO(crbug.com/324104311): Fix flaky test.
IN_PROC_BROWSER_TEST_F(DeviceTrustCreateKeyUploadFailedBrowserTest,
                       DISABLED_AttestationFullFlowSucceedOnThirdAttempt) {
  ASSERT_FALSE(device_trust_test_environment_win_->KeyExists());
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

class DeviceTrustKeyRotationBrowserTest : public DeviceTrustDesktopBrowserTest {
 protected:
  DeviceTrustKeyRotationBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            kDTCKeyUploadedBySharedAPIEnabled,
        },
        /*disabled_features=*/{kDTCKeyRotationEnabled});
  }
};

IN_PROC_BROWSER_TEST_F(DeviceTrustKeyRotationBrowserTest,
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

// Flaky on Win. See http://crbug.com/324937427.
#if BUILDFLAG(IS_WIN)
#define MAYBE_RemoteCommandKeyRotationFailure \
  DISABLED_RemoteCommandKeyRotationFailure
#else
#define MAYBE_RemoteCommandKeyRotationFailure RemoteCommandKeyRotationFailure
#endif
IN_PROC_BROWSER_TEST_F(DeviceTrustKeyRotationBrowserTest,
                       MAYBE_RemoteCommandKeyRotationFailure) {
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

#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

class DeviceTrustBrowserTestWithConsent
    : public InteractiveBrowserTestT<DeviceTrustBrowserTest>,
      public testing::WithParamInterface<
          /* Six boolean variables that define the general consent:
          - if the managed profile and device are affiliated
          - if the user is managed
          - if user-level inline flow is enabled
          - if the device is managed
          - if device-level inline flow is enabled
          - if UnmanagedDeviceSignalsConsentFlowEnabled policy is enabled */
          testing::tuple<bool, bool, bool, bool, bool, bool>> {
 protected:
  DeviceTrustBrowserTestWithConsent()
      : InteractiveBrowserTestT(DeviceTrustConnectorState({
            .affiliated = testing::get<0>(GetParam()),
            .cloud_user_management_level = DeviceTrustManagementLevel({
                .is_managed = testing::get<1>(GetParam()),
                .is_inline_policy_enabled = testing::get<2>(GetParam()),
            }),
            .cloud_machine_management_level = DeviceTrustManagementLevel({
                .is_managed = testing::get<3>(GetParam()),
                .is_inline_policy_enabled = testing::get<4>(GetParam()),
            }),
        })) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            enterprise_signals::features::kDeviceSignalsConsentDialog,
            kDTCKeyUploadedBySharedAPIEnabled,
#if BUILDFLAG(IS_MAC)
            kDTCKeyRotationUploadedBySharedAPIEnabled,
#endif  // BUILDFLAG(IS_MAC)
        },
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTestT::SetUpOnMainThread();

    browser()->profile()->GetPrefs()->SetBoolean(
        device_signals::prefs::kUnmanagedDeviceSignalsConsentFlowEnabled,
        is_consent_policy_enabled());
  }

  void NavigateWithUserGesture() {
    GURL redirect_url = GetRedirectUrl();
    pending_navigation_.emplace(web_contents(), redirect_url);

    content::NavigationController::LoadURLParams params(redirect_url);
    params.transition_type =
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED);
    params.has_user_gesture = true;
    web_contents()->GetController().LoadURLWithParams(params);
  }

  void WaitForNavigation() {
    ASSERT_TRUE(pending_navigation_);
    ASSERT_TRUE(pending_navigation_->WaitForNavigationFinished());
  }

  void AcceptConsentDialog() {
    // Wait for consent dialog to be entirely ready.
    base::RunLoop().RunUntilIdle();

    RunTestSequence(
        InAnyContext(WaitForShow(kDeviceSignalsConsentOkButtonElementId)),
        InSameContext(
            Steps(PressButton(kDeviceSignalsConsentOkButtonElementId),
                  WaitForHide(kDeviceSignalsConsentOkButtonElementId))));

    WaitForNavigation();
  }

  bool is_affiliated() { return testing::get<0>(GetParam()); }
  bool is_profile_managed() { return testing::get<1>(GetParam()); }
  bool is_user_inline_flow_enabled() { return testing::get<2>(GetParam()); }
  bool is_device_managed() { return testing::get<3>(GetParam()); }
  bool is_device_inline_flow_enabled() { return testing::get<4>(GetParam()); }
  bool is_consent_policy_enabled() { return testing::get<5>(GetParam()); }

  virtual bool ShouldTriggerConsent() {
    if ((is_device_managed() && is_affiliated()) || !is_profile_managed()) {
      return false;
    }
    return ((is_consent_policy_enabled() && !is_device_managed()) ||
            is_user_inline_flow_enabled());
  }

  std::optional<enterprise_connectors::DTAttestationPolicyLevel>
  GetExpectedAttestationPolicyLevel() {
    if (is_user_inline_flow_enabled() && is_device_inline_flow_enabled()) {
      return enterprise_connectors::DTAttestationPolicyLevel::kUserAndBrowser;
    }
    if (is_user_inline_flow_enabled()) {
      return enterprise_connectors::DTAttestationPolicyLevel::kUser;
    }
    if (is_device_inline_flow_enabled()) {
      return enterprise_connectors::DTAttestationPolicyLevel::kBrowser;
    }
    return std::nullopt;
  }

  std::optional<content::TestNavigationManager> pending_navigation_;
};

IN_PROC_BROWSER_TEST_P(DeviceTrustBrowserTestWithConsent,
                       ConsentDialogWithPolicyAndAttestation) {
  NavigateWithUserGesture();

  DTAttestationResult success_result =
      is_device_inline_flow_enabled()
          ? DTAttestationResult::kSuccess
          : DTAttestationResult::kSuccessNoSignature;

  std::optional<enterprise_connectors::DTAttestationPolicyLevel> policy_level =
      GetExpectedAttestationPolicyLevel();

  if (ShouldTriggerConsent()) {
    AcceptConsentDialog();
    policy_level ? VerifyAttestationFlowSuccessful(success_result, policy_level)
                 : VerifyNoInlineFlowOccurred();

    ResetState();
    NavigateWithUserGesture();
  }

  RunTestSequence(EnsureNotPresent(kDeviceSignalsConsentOkButtonElementId));
  WaitForNavigation();

  policy_level ? VerifyAttestationFlowSuccessful(success_result, policy_level)
               : VerifyNoInlineFlowOccurred();

  /* This section can be considered as an extra test case, it's implemented here
  due to the high difficulty and large amount of codes required for a separate
  test case set up.

  In this test case, both user and device are unmanaged so
  consent collection and attestation shouldn't happen. However, if a user
  becomes managed with DTC enabled, consent collection should happen during the
  next eligible navigation, and user-level attestation can be triggered.
  */
  if (!is_profile_managed() && !is_device_managed()) {
    device_trust_mixin_->ManageCloudUser();
    device_trust_mixin_->EnableUserInlinePolicy();

    NavigateWithUserGesture();
    AcceptConsentDialog();
    VerifyAttestationFlowSuccessful(success_result, policy_level);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ManagedUserAndUnmanagedDevice,
    DeviceTrustBrowserTestWithConsent,
    testing::Combine(/*is_affiliated=*/testing::Values(true),
                     /*is_profile_managed=*/testing::Values(true),
                     /*is_user_inline_flow_enabled=*/testing::Bool(),
                     /*is_device_managed=*/testing::Values(false),
                     /*is_device_inline_flow_enabled=*/testing::Values(false),
                     /*is_consent_policy_enabled=*/testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    ManagedUserAndManagedDevice,
    DeviceTrustBrowserTestWithConsent,
    testing::Combine(/*is_affiliated=*/testing::Bool(),
                     /*is_profile_managed=*/testing::Values(true),
                     /*is_user_inline_flow_enabled=*/testing::Bool(),
                     /*is_device_managed=*/testing::Values(true),
                     /*is_device_inline_flow_enabled=*/testing::Bool(),
                     /*is_consent_policy_enabled=*/testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    UnmanagedUserAndManagedDevice,
    DeviceTrustBrowserTestWithConsent,
    testing::Combine(/*is_affiliated=*/testing::Values(true),
                     /*is_profile_managed=*/testing::Values(false),
                     /*is_user_inline_flow_enabled=*/testing::Values(false),
                     /*is_device_managed=*/testing::Values(true),
                     /*is_device_inline_flow_enabled=*/testing::Bool(),
                     /*is_consent_policy_enabled=*/testing::Values(false)));

INSTANTIATE_TEST_SUITE_P(
    UnmanagedUserAndUnmanagedDevice,
    DeviceTrustBrowserTestWithConsent,
    testing::Combine(/*is_affiliated=*/testing::Values(true),
                     /*is_profile_managed=*/testing::Values(false),
                     /*is_user_inline_flow_enabled=*/testing::Values(false),
                     /*is_device_managed=*/testing::Values(false),
                     /*is_device_inline_flow_enabled=*/testing::Values(false),
                     /*is_consent_policy_enabled=*/testing::Values(false)));

class DeviceTrustBrowserTestWithPermanentConsent
    : public DeviceTrustBrowserTestWithConsent {
 protected:
  DeviceTrustBrowserTestWithPermanentConsent() = default;

  void SetUpOnMainThread() override {
    DeviceTrustBrowserTestWithConsent::SetUpOnMainThread();
    device_trust_mixin_->SetPermanentConsentGiven(true);
  }
};

IN_PROC_BROWSER_TEST_P(DeviceTrustBrowserTestWithPermanentConsent,
                       ConsentDialogWithPolicyAndAttestation) {
  NavigateWithUserGesture();

  DTAttestationResult success_result =
      is_device_inline_flow_enabled()
          ? DTAttestationResult::kSuccess
          : DTAttestationResult::kSuccessNoSignature;

  std::optional<enterprise_connectors::DTAttestationPolicyLevel> policy_level =
      GetExpectedAttestationPolicyLevel();

  RunTestSequence(EnsureNotPresent(kDeviceSignalsConsentOkButtonElementId));
  WaitForNavigation();

  policy_level ? VerifyAttestationFlowSuccessful(success_result, policy_level)
               : VerifyNoInlineFlowOccurred();

  // Test case where the user becomes managed with DTC enabled.
  if (!is_profile_managed() && !is_device_managed()) {
    device_trust_mixin_->ManageCloudUser();
    device_trust_mixin_->EnableUserInlinePolicy();

    NavigateWithUserGesture();
    WaitForNavigation();
    VerifyAttestationFlowSuccessful(success_result, policy_level);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ManagedUserAndUnmanagedDevice,
    DeviceTrustBrowserTestWithPermanentConsent,
    testing::Combine(/*is_affiliated=*/testing::Values(true),
                     /*is_profile_managed=*/testing::Values(true),
                     /*is_user_inline_flow_enabled=*/testing::Bool(),
                     /*is_device_managed=*/testing::Values(false),
                     /*is_device_inline_flow_enabled=*/testing::Values(false),
                     /*is_consent_policy_enabled=*/testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    ManagedUserAndManagedDevice,
    DeviceTrustBrowserTestWithPermanentConsent,
    testing::Combine(/*is_affiliated=*/testing::Bool(),
                     /*is_profile_managed=*/testing::Values(true),
                     /*is_user_inline_flow_enabled=*/testing::Bool(),
                     /*is_device_managed=*/testing::Values(true),
                     /*is_device_inline_flow_enabled=*/testing::Bool(),
                     /*is_consent_policy_enabled=*/testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    UnmanagedUserAndManagedDevice,
    DeviceTrustBrowserTestWithPermanentConsent,
    testing::Combine(/*is_affiliated=*/testing::Values(true),
                     /*is_profile_managed=*/testing::Values(false),
                     /*is_user_inline_flow_enabled=*/testing::Values(false),
                     /*is_device_managed=*/testing::Values(true),
                     /*is_device_inline_flow_enabled=*/testing::Bool(),
                     /*is_consent_policy_enabled=*/testing::Values(false)));

INSTANTIATE_TEST_SUITE_P(
    UnmanagedUserAndUnmanagedDevice,
    DeviceTrustBrowserTestWithPermanentConsent,
    testing::Combine(/*is_affiliated=*/testing::Values(true),
                     /*is_profile_managed=*/testing::Values(false),
                     /*is_user_inline_flow_enabled=*/testing::Values(false),
                     /*is_device_managed=*/testing::Values(false),
                     /*is_device_inline_flow_enabled=*/testing::Values(false),
                     /*is_consent_policy_enabled=*/testing::Values(false)));

class DeviceTrustPolicyLevelBrowserTest
    : public DeviceTrustBrowserTest,
      public testing::WithParamInterface<
          /* Three boolean variables that define the general consent:
          - if the managed profile and device are affiliated
          - if the device-level inline flow will be triggered
          - if the user-level inline flow will be triggered */
          testing::tuple<bool, bool, bool>> {
 protected:
  DeviceTrustPolicyLevelBrowserTest()
      : DeviceTrustBrowserTest(DeviceTrustConnectorState({
            .affiliated = testing::get<0>(GetParam()),
            .cloud_user_management_level = DeviceTrustManagementLevel({
                .is_managed = true,
                .is_inline_policy_enabled = false,
            }),
            .cloud_machine_management_level = DeviceTrustManagementLevel({
                .is_managed = true,
                .is_inline_policy_enabled = false,
            }),
        })) {
    scoped_feature_list_.InitWithFeatureState(
        enterprise_signals::features::kDeviceSignalsConsentDialog, true);
  }

  bool is_affiliated() { return testing::get<0>(GetParam()); }
  bool will_trigger_device_inline_flow() { return testing::get<1>(GetParam()); }
  bool will_trigger_user_inline_flow() { return testing::get<2>(GetParam()); }

  void SetUpOnMainThread() override {
    DeviceTrustBrowserTest::SetUpOnMainThread();

    device_trust_mixin_->SetConsentGiven(true);
    (will_trigger_device_inline_flow())
        ? device_trust_mixin_->EnableMachineInlinePolicy()
        : device_trust_mixin_->EnableMachineInlinePolicy(kOtherHost);
    (will_trigger_user_inline_flow())
        ? device_trust_mixin_->EnableUserInlinePolicy()
        : device_trust_mixin_->EnableUserInlinePolicy(kOtherHost);
  }
};

IN_PROC_BROWSER_TEST_P(DeviceTrustPolicyLevelBrowserTest,
                       AttestationPolicyLevelTest) {
  TriggerUrlNavigation();

  DTAttestationResult success_result =
      will_trigger_device_inline_flow()
          ? DTAttestationResult::kSuccess
          : DTAttestationResult::kSuccessNoSignature;

  if (will_trigger_device_inline_flow() && will_trigger_user_inline_flow()) {
    VerifyAttestationFlowSuccessful(
        success_result,
        enterprise_connectors::DTAttestationPolicyLevel::kUserAndBrowser);
  } else if (will_trigger_device_inline_flow()) {
    VerifyAttestationFlowSuccessful(
        success_result,
        enterprise_connectors::DTAttestationPolicyLevel::kBrowser);
  } else if (will_trigger_user_inline_flow()) {
    VerifyAttestationFlowSuccessful(
        success_result, enterprise_connectors::DTAttestationPolicyLevel::kUser);
  } else {
    VerifyNoInlineFlowOccurred();
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceTrustPolicyLevelBrowserTest,
    testing::Combine(/*is_affiliated=*/testing::Bool(),
                     /*will_trigger_device_inline_flow=*/testing::Bool(),
                     /*will_trigger_user_inline_flow=*/testing::Bool()));

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS_ASH)

class DeviceTrustBrowserTestForUnmanagedDevices
    : public DeviceTrustBrowserTest,
      public testing::WithParamInterface<
          /* 3 boolean variables that define the flow on unmanaged devices
          (crOS):
          - if the user is managed
          - if user-level inline flow is enabled
          - if UnmanagedDeviceDeviceTrustConnectorEnabled feature is enabled*/
          testing::tuple<bool, bool, bool>> {
 protected:
  DeviceTrustBrowserTestForUnmanagedDevices()
      : DeviceTrustBrowserTest(DeviceTrustConnectorState({
            .affiliated = false,
            .cloud_user_management_level = DeviceTrustManagementLevel({
                .is_managed = testing::get<0>(GetParam()),
                .is_inline_policy_enabled = testing::get<1>(GetParam()),
            }),
        })) {
    scoped_feature_list_.InitWithFeatureState(
        ash::features::kUnmanagedDeviceDeviceTrustConnectorEnabled,
        is_unmanaged_device_feature_enabled());
  }

  bool is_user_managed() { return testing::get<0>(GetParam()); }
  bool is_user_inline_flow_enabled() { return testing::get<1>(GetParam()); }
  bool is_unmanaged_device_feature_enabled() {
    return testing::get<2>(GetParam());
  }
};

IN_PROC_BROWSER_TEST_P(DeviceTrustBrowserTestForUnmanagedDevices,
                       AttestationFullFlow) {
  TriggerUrlNavigation();

  if (!is_unmanaged_device_feature_enabled() || !is_user_managed() ||
      !is_user_inline_flow_enabled()) {
    VerifyNoInlineFlowOccurred();
    return;
  }

  VerifyAttestationFlowSuccessful();
}

INSTANTIATE_TEST_SUITE_P(
    ManagedUser,
    DeviceTrustBrowserTestForUnmanagedDevices,
    testing::Combine(
        /*is_user_managed=*/testing::Values(true),
        /*is_user_inline_flow_enabled=*/testing::Bool(),
        /*is_unmanaged_device_feature_enabled=*/testing::Values(true)));
INSTANTIATE_TEST_SUITE_P(
    UnmanagedUser,
    DeviceTrustBrowserTestForUnmanagedDevices,
    testing::Combine(
        /*is_user_managed=*/testing::Values(false),
        /*is_user_inline_flow_enabled=*/testing::Values(false),
        /*is_unmanaged_device_feature_enabled=*/testing::Values(true)));

INSTANTIATE_TEST_SUITE_P(
    FeatureFlag,
    DeviceTrustBrowserTestForUnmanagedDevices,
    testing::Combine(
        /*is_user_managed=*/testing::Values(true),
        /*is_user_inline_flow_enabled=*/testing::Values(true),
        /*is_unmanaged_device_feature_enabled=*/testing::Values(true)));

class DeviceTrustBrowserTestSignalsContractForUnmanagedDevices
    : public DeviceTrustBrowserTest {
 protected:
  DeviceTrustBrowserTestSignalsContractForUnmanagedDevices()
      : DeviceTrustBrowserTest(DeviceTrustConnectorState({
            .affiliated = false,
            .cloud_user_management_level = DeviceTrustManagementLevel({
                .is_managed = true,
                .is_inline_policy_enabled = true,
            }),
        })) {
    scoped_feature_list_.InitWithFeatureState(
        ash::features::kUnmanagedDeviceDeviceTrustConnectorEnabled, true);
  }
};

// Tests that signal values respect the expected format and is filled-out
// as expect, especially respective filtered stable device identifiers.
IN_PROC_BROWSER_TEST_F(DeviceTrustBrowserTestSignalsContractForUnmanagedDevices,
                       SignalsContract) {
  auto* device_trust_service =
      DeviceTrustServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(device_trust_service);

  base::test::TestFuture<base::Value::Dict> future;
  device_trust_service->GetSignals(future.GetCallback());

  // This error most likely indicates that one of the signals decorators did
  // not invoke its done_closure in time.
  ASSERT_TRUE(future.Wait()) << "Timed out while collecting signals.";

  const base::Value::Dict& signals_dict = future.Get();

  const auto signals_contract_map =
      device_signals::test::GetSignalsContractForUnmanagedDevices();
  ASSERT_FALSE(signals_contract_map.empty());
  for (const auto& signals_contract_entry : signals_contract_map) {
    // First is the signal name.
    // Second is the contract evaluation predicate.
    EXPECT_TRUE(signals_contract_entry.second.Run(signals_dict))
        << "Signals contract validation failed for: "
        << signals_contract_entry.first;
  }
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace enterprise_connectors::test
