// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/qr_code.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/session_context.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/mock_second_device_auth_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/oobe_quick_start_pref_names.h"
#include "chrome/browser/ash/login/oobe_quick_start/second_device_auth_broker.h"
#include "chrome/browser/ash/nearby/fake_quick_start_connectivity_service.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
#include "chromeos/ash/components/quick_start/types.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash::quick_start {

namespace {

using Observer = TargetDeviceBootstrapController::Observer;
using Status = TargetDeviceBootstrapController::Status;
using Step = TargetDeviceBootstrapController::Step;
using Payload = TargetDeviceBootstrapController::Payload;
using ErrorCode = TargetDeviceBootstrapController::ErrorCode;
using ConnectionClosedReason =
    TargetDeviceConnectionBroker::ConnectionClosedReason;

constexpr char kWifiTransferResultHistogramName[] =
    "QuickStart.WifiTransferResult";
constexpr char kGaiaTransferResultHistogramName[] =
    "QuickStart.GaiaTransferResult";
constexpr char kGaiaTransferResultFailureReasonHistogramName[] =
    "QuickStart.GaiaTransferResult.FailureReason";

class FakeObserver : public Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  void OnStatusChanged(const Status& status) override {
    // Step must change.
    ASSERT_NE(status.step, last_status.step);

    last_status = status;
    num_on_status_changed_called++;
  }

  Status last_status;
  int num_on_status_changed_called = 0;
};

constexpr char kFakeChallengeBytesBase64[] =
    "ABz12ClFhY8/D89zWFB+KTHgUwJ5T3Avco/1IQuu+K/"
    "65KlsmB7o0+UyPde8ZW+b33aeJ9uyST8EMzS6WhK60e/VDjug+7LLK4YzDz1nNw==";
constexpr char kTestCredentialId[] = "TEST_CREDENTIAL_ID";
constexpr char kTestAuthCode[] = "AUTHORIZATION_CODE";
constexpr char kPemCertificateString[] = R"({
-----BEGIN CERTIFICATE-----
MIICUTCCAfugAwIBAgIBADANBgkqhkiG9w0BAQQFADBXMQswCQYDVQQGEwJDTjEL
MAkGA1UECBMCUE4xCzAJBgNVBAcTAkNOMQswCQYDVQQKEwJPTjELMAkGA1UECxMC
VU4xFDASBgNVBAMTC0hlcm9uZyBZYW5nMB4XDTA1MDcxNTIxMTk0N1oXDTA1MDgx
NDIxMTk0N1owVzELMAkGA1UEBhMCQ04xCzAJBgNVBAgTAlBOMQswCQYDVQQHEwJD
TjELMAkGA1UEChMCT04xCzAJBgNVBAsTAlVOMRQwEgYDVQQDEwtIZXJvbmcgWWFu
ZzBcMA0GCSqGSIb3DQEBAQUAA0sAMEgCQQCp5hnG7ogBhtlynpOS21cBewKE/B7j
V14qeyslnr26xZUsSVko36ZnhiaO/zbMOoRcKK9vEcgMtcLFuQTWDl3RAgMBAAGj
gbEwga4wHQYDVR0OBBYEFFXI70krXeQDxZgbaCQoR4jUDncEMH8GA1UdIwR4MHaA
FFXI70krXeQDxZgbaCQoR4jUDncEoVukWTBXMQswCQYDVQQGEwJDTjELMAkGA1UE
CBMCUE4xCzAJBgNVBAcTAkNOMQswCQYDVQQKEwJPTjELMAkGA1UECxMCVU4xFDAS
BgNVBAMTC0hlcm9uZyBZYW5nggEAMAwGA1UdEwQFMAMBAf8wDQYJKoZIhvcNAQEE
BQADQQA/ugzBrjjK9jcWnDVfGHlk3icNRq0oV7Ri32z/+HQX67aRfgZu7KWdI+Ju
Wm7DCfrPNGVwFWUQOmsPue9rZBgO
-----END CERTIFICATE-----
    })";

class FakeAccessibilityManagerWrapper
    : public TargetDeviceBootstrapController::AccessibilityManagerWrapper {
 public:
  bool AllowQRCodeUX() const override { return allow_qr_code_ux_; }

  bool allow_qr_code_ux_ = true;
};

}  // namespace

class TargetDeviceBootstrapControllerTest : public testing::Test {
 public:
  using MockAuthBroker = testing::NiceMock<MockSecondDeviceAuthBroker>;

  static constexpr char kSourceDeviceId[] = "fake-source-device-id";

  TargetDeviceBootstrapControllerTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}

  TargetDeviceBootstrapControllerTest(TargetDeviceBootstrapControllerTest&) =
      delete;
  TargetDeviceBootstrapControllerTest& operator=(
      TargetDeviceBootstrapControllerTest&) = delete;
  ~TargetDeviceBootstrapControllerTest() override = default;

  void SetUp() override { CreateBootstrapController(); }
  void TearDown() override {
    auth_broker_ = nullptr;
    bootstrap_controller_->RemoveObserver(fake_observer_.get());
    TargetDeviceConnectionBrokerFactory::SetFactoryForTesting(nullptr);
  }

  void CreateBootstrapController() {
    auto auth_broker =
        std::make_unique<MockAuthBroker>(test_factory_.GetSafeWeakWrapper());
    auth_broker_ = auth_broker.get();
    auto fake_accessibility_manager =
        std::make_unique<FakeAccessibilityManagerWrapper>();
    fake_accessibility_manager_ = fake_accessibility_manager.get();
    fake_quick_start_connectivity_service_ =
        std::make_unique<FakeQuickStartConnectivityService>();

    bootstrap_controller_ = std::make_unique<TargetDeviceBootstrapController>(
        std::move(auth_broker), std::move(fake_accessibility_manager),
        fake_quick_start_connectivity_service_.get());

    std::unique_ptr<FakeTargetDeviceConnectionBroker>
        fake_target_device_connection_broker =
            std::make_unique<FakeTargetDeviceConnectionBroker>(
                &bootstrap_controller_->session_context_,
                fake_quick_start_connectivity_service_.get());
    fake_target_device_connection_broker_ =
        fake_target_device_connection_broker.get();
    bootstrap_controller_->set_connection_broker_for_testing(
        std::move(fake_target_device_connection_broker));

    fake_observer_ = std::make_unique<FakeObserver>();
    bootstrap_controller_->AddObserver(fake_observer_.get());
  }

  void BootstrapConnection() {
    bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
    fake_target_device_connection_broker_->on_start_advertising_callback().Run(
        /*success=*/true);
    fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
    fake_target_device_connection_broker_->AuthenticateConnection(
        kSourceDeviceId, QuickStartMetrics::AuthenticationMethod::kQRCode);

    ASSERT_EQ(fake_observer_->last_status.step, Step::ADVERTISING_WITH_QR_CODE);
  }

  void NotifySourceOfUpdateResponse(bool ack_successful) {
    bootstrap_controller_->OnNotifySourceOfUpdateResponse(ack_successful);
  }

  PrefService* GetLocalState() { return local_state_.Get(); }

  void ExpectQuickStartConnectivityServiceCleanupCalled() {
    EXPECT_TRUE(
        fake_quick_start_connectivity_service_->get_is_cleanup_called());
  }

  SessionContext* GetSessionContext() {
    return &bootstrap_controller_->session_context_;
  }

  void UpdateStatus(Step step, Payload payload) {
    bootstrap_controller_->UpdateStatus(step, payload);
  }

  void ResumeAfterUpdate() {
    EXPECT_FALSE(GetSessionContext()->is_resume_after_update());
    GetLocalState()->SetDict(prefs::kResumeQuickStartAfterRebootInfo,
                             GetSessionContext()->GetPrepareForUpdateInfo());

    auth_broker_ = nullptr;
    bootstrap_controller_.reset();
    CreateBootstrapController();
  }

 protected:
  std::optional<FidoAssertionInfo> assertion_info_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_factory_;
  std::unique_ptr<FakeQuickStartConnectivityService>
      fake_quick_start_connectivity_service_;
  raw_ptr<FakeTargetDeviceConnectionBroker, DanglingUntriaged>
      fake_target_device_connection_broker_;
  std::unique_ptr<FakeObserver> fake_observer_;
  raw_ptr<MockAuthBroker> auth_broker_;
  raw_ptr<FakeAccessibilityManagerWrapper, DanglingUntriaged>
      fake_accessibility_manager_ = nullptr;
  std::unique_ptr<TargetDeviceBootstrapController> bootstrap_controller_;
  ScopedTestingLocalState local_state_;
  base::HistogramTester histogram_tester_;
  const Base64UrlString kFakeChallengeBytes_ =
      *Base64UrlTranscode(Base64String(kFakeChallengeBytesBase64));
};

TEST_F(TargetDeviceBootstrapControllerTest, StartAdvertisingAndMaybeGetQRCode) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  EXPECT_EQ(
      1u, fake_target_device_connection_broker_->num_start_advertising_calls());
  ASSERT_TRUE(fake_target_device_connection_broker_
                  ->start_advertising_use_pin_authentication()
                  .has_value());
  EXPECT_FALSE(fake_target_device_connection_broker_
                   ->start_advertising_use_pin_authentication()
                   .value());
  EXPECT_EQ(
      bootstrap_controller_.get(),
      fake_target_device_connection_broker_->connection_lifecycle_listener());

  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  EXPECT_EQ(fake_observer_->last_status.step, Step::ADVERTISING_WITH_QR_CODE);
  EXPECT_TRUE(
      absl::holds_alternative<QRCode>(fake_observer_->last_status.payload));
}

TEST_F(TargetDeviceBootstrapControllerTest,
       StartAdvertisingAndMaybeGetQRCodeFail) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/false);
  EXPECT_EQ(fake_observer_->last_status.step, Step::ERROR);
  ASSERT_TRUE(
      absl::holds_alternative<ErrorCode>(fake_observer_->last_status.payload));
  EXPECT_EQ(absl::get<ErrorCode>(fake_observer_->last_status.payload),
            ErrorCode::START_ADVERTISING_FAILED);
  ExpectQuickStartConnectivityServiceCleanupCalled();
}

TEST_F(TargetDeviceBootstrapControllerTest,
       StartAdvertisingWithChromevoxUsesPin) {
  fake_accessibility_manager_->allow_qr_code_ux_ = false;
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  EXPECT_EQ(fake_observer_->last_status.step,
            Step::ADVERTISING_WITHOUT_QR_CODE);
  EXPECT_FALSE(
      absl::holds_alternative<QRCode>(fake_observer_->last_status.payload));
  EXPECT_EQ(
      1u, fake_target_device_connection_broker_->num_start_advertising_calls());
  ASSERT_TRUE(fake_target_device_connection_broker_
                  ->start_advertising_use_pin_authentication()
                  .has_value());
  EXPECT_TRUE(fake_target_device_connection_broker_
                  ->start_advertising_use_pin_authentication()
                  .value());
}

TEST_F(TargetDeviceBootstrapControllerTest, StopAdvertising) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  ASSERT_EQ(fake_observer_->last_status.step, Step::ADVERTISING_WITH_QR_CODE);

  bootstrap_controller_->StopAdvertising();
  EXPECT_EQ(
      1u, fake_target_device_connection_broker_->num_stop_advertising_calls());

  // Status changes only after the `on_stop_advertising_callback` run.
  EXPECT_EQ(fake_observer_->last_status.step, Step::ADVERTISING_WITH_QR_CODE);

  fake_target_device_connection_broker_->on_stop_advertising_callback().Run();
  EXPECT_EQ(fake_observer_->last_status.step, Step::NONE);
  ExpectQuickStartConnectivityServiceCleanupCalled();
}

TEST_F(TargetDeviceBootstrapControllerTest, StopAdvertisingAfterConnection) {
  BootstrapConnection();

  fake_target_device_connection_broker_->GetFakeConnection()->VerifyUser(
      mojom::UserVerificationResponse(
          mojom::UserVerificationResult::kUserVerified,
          /*is_first_user_verification=*/true));
  EXPECT_EQ(fake_observer_->last_status.step, Step::CONNECTED);

  bootstrap_controller_->StopAdvertising();
  fake_target_device_connection_broker_->on_stop_advertising_callback().Run();

  // Status shouldn't change since we have a connection.
  EXPECT_EQ(fake_observer_->last_status.step, Step::CONNECTED);
  EXPECT_FALSE(fake_quick_start_connectivity_service_->get_is_cleanup_called());
}

TEST_F(TargetDeviceBootstrapControllerTest, InitiateConnection_QRCode) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  ASSERT_EQ(fake_observer_->last_status.step, Step::ADVERTISING_WITH_QR_CODE);
  EXPECT_TRUE(
      absl::holds_alternative<QRCode>(fake_observer_->last_status.payload));

  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  // Status shouldn't change.
  EXPECT_EQ(fake_observer_->last_status.step, Step::ADVERTISING_WITH_QR_CODE);
  EXPECT_TRUE(
      absl::holds_alternative<QRCode>(fake_observer_->last_status.payload));
}

TEST_F(TargetDeviceBootstrapControllerTest, InitiateConnection_Pin) {
  fake_accessibility_manager_->allow_qr_code_ux_ = false;
  fake_target_device_connection_broker_->set_use_pin_authentication(true);
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  ASSERT_EQ(fake_observer_->last_status.step,
            Step::ADVERTISING_WITHOUT_QR_CODE);

  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);

  EXPECT_EQ(fake_observer_->last_status.step, Step::PIN_VERIFICATION);
  EXPECT_TRUE(
      absl::holds_alternative<PinString>(fake_observer_->last_status.payload));
  EXPECT_TRUE(
      absl::get<PinString>(fake_observer_->last_status.payload)->length() == 4);
}

TEST_F(TargetDeviceBootstrapControllerTest, AuthenticateConnection) {
  BootstrapConnection();
  EXPECT_TRUE(
      absl::holds_alternative<QRCode>(fake_observer_->last_status.payload));
}

TEST_F(TargetDeviceBootstrapControllerTest, FeatureSupportStatus) {
  std::optional<TargetDeviceConnectionBroker::FeatureSupportStatus>
      feature_status;

  fake_target_device_connection_broker_->set_feature_support_status(
      FakeTargetDeviceConnectionBroker::FeatureSupportStatus::kUndetermined);

  bootstrap_controller_->GetFeatureSupportStatusAsync(
      base::BindLambdaForTesting(
          [&](TargetDeviceConnectionBroker::FeatureSupportStatus status) {
            feature_status = status;
          }));
  EXPECT_FALSE(feature_status.has_value());

  fake_target_device_connection_broker_->set_feature_support_status(
      FakeTargetDeviceConnectionBroker::FeatureSupportStatus::kNotSupported);
  ASSERT_TRUE(feature_status.has_value());
  EXPECT_EQ(
      feature_status.value(),
      FakeTargetDeviceConnectionBroker::FeatureSupportStatus::kNotSupported);
}

TEST_F(TargetDeviceBootstrapControllerTest, RejectConnection) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);

  fake_target_device_connection_broker_->RejectConnection();

  EXPECT_EQ(fake_observer_->last_status.step, Step::ERROR);
  ASSERT_TRUE(
      absl::holds_alternative<ErrorCode>(fake_observer_->last_status.payload));
  EXPECT_EQ(absl::get<ErrorCode>(fake_observer_->last_status.payload),
            ErrorCode::CONNECTION_REJECTED);
}

TEST_F(TargetDeviceBootstrapControllerTest, CloseConnection) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);

  fake_target_device_connection_broker_->CloseConnection(
      ConnectionClosedReason::kUnknownError);

  EXPECT_EQ(fake_observer_->last_status.step, Step::ERROR);
  ASSERT_TRUE(
      absl::holds_alternative<ErrorCode>(fake_observer_->last_status.payload));
  EXPECT_EQ(absl::get<ErrorCode>(fake_observer_->last_status.payload),
            ErrorCode::CONNECTION_CLOSED);
}

TEST_F(TargetDeviceBootstrapControllerTest, GetPhoneInstanceId) {
  // Authenticate connection.
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  fake_target_device_connection_broker_->AuthenticateConnection(
      kSourceDeviceId, QuickStartMetrics::AuthenticationMethod::kQRCode);

  // Set phone instance ID.
  std::vector<uint8_t> phone_instance_id = {0x01, 0x02, 0x03};
  std::string expected_phone_instance_id(phone_instance_id.begin(),
                                         phone_instance_id.end());
  fake_target_device_connection_broker_->GetFakeConnection()
      ->set_phone_instance_id(expected_phone_instance_id);

  EXPECT_EQ(bootstrap_controller_->GetPhoneInstanceId(),
            expected_phone_instance_id);
}

TEST_F(TargetDeviceBootstrapControllerTest,
       OnNotifySourceOfUpdateResponse_AckSuccessful) {
  ASSERT_FALSE(
      GetLocalState()->GetBoolean(prefs::kShouldResumeQuickStartAfterReboot));
  ASSERT_TRUE(GetLocalState()
                  ->GetDict(prefs::kResumeQuickStartAfterRebootInfo)
                  .empty());
  BootstrapConnection();

  NotifySourceOfUpdateResponse(/*ack_successful=*/true);

  ASSERT_TRUE(
      absl::holds_alternative<ErrorCode>(fake_observer_->last_status.payload));
  EXPECT_EQ(absl::get<ErrorCode>(fake_observer_->last_status.payload),
            ErrorCode::CONNECTION_CLOSED);
  EXPECT_TRUE(
      GetLocalState()->GetBoolean(prefs::kShouldResumeQuickStartAfterReboot));
  EXPECT_FALSE(GetLocalState()
                   ->GetDict(prefs::kResumeQuickStartAfterRebootInfo)
                   .empty());
}

TEST_F(TargetDeviceBootstrapControllerTest,
       OnNotifySourceOfUpdateResponse_AckUnsuccessful) {
  ASSERT_FALSE(
      GetLocalState()->GetBoolean(prefs::kShouldResumeQuickStartAfterReboot));
  ASSERT_TRUE(GetLocalState()
                  ->GetDict(prefs::kResumeQuickStartAfterRebootInfo)
                  .empty());
  BootstrapConnection();

  NotifySourceOfUpdateResponse(/*ack_successful=*/false);

  ASSERT_TRUE(
      absl::holds_alternative<ErrorCode>(fake_observer_->last_status.payload));
  EXPECT_EQ(absl::get<ErrorCode>(fake_observer_->last_status.payload),
            ErrorCode::CONNECTION_CLOSED);
  EXPECT_FALSE(
      GetLocalState()->GetBoolean(prefs::kShouldResumeQuickStartAfterReboot));
  EXPECT_TRUE(GetLocalState()
                  ->GetDict(prefs::kResumeQuickStartAfterRebootInfo)
                  .empty());
}

TEST_F(TargetDeviceBootstrapControllerTest, RequestWifiCredentials) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  fake_target_device_connection_broker_->AuthenticateConnection(
      kSourceDeviceId, QuickStartMetrics::AuthenticationMethod::kQRCode);

  EXPECT_EQ(fake_observer_->last_status.step, Step::ADVERTISING_WITH_QR_CODE);
  EXPECT_TRUE(
      absl::holds_alternative<QRCode>(fake_observer_->last_status.payload));

  fake_target_device_connection_broker_->GetFakeConnection()->VerifyUser(
      mojom::UserVerificationResponse(
          mojom::UserVerificationResult::kUserVerified,
          /*is_first_user_verification=*/true));

  EXPECT_EQ(fake_observer_->last_status.step, Step::CONNECTED);
  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(
      fake_observer_->last_status.payload));

  bootstrap_controller_->AttemptWifiCredentialTransfer();
  fake_target_device_connection_broker_->GetFakeConnection()
      ->SendWifiCredentials(
          mojom::WifiCredentials("ssid", mojom::WifiSecurityType::kWEP,
                                 /*is_hidden=*/true, "password"));

  EXPECT_EQ(fake_observer_->last_status.step, Step::WIFI_CREDENTIALS_RECEIVED);
  EXPECT_TRUE(absl::holds_alternative<mojom::WifiCredentials>(
      fake_observer_->last_status.payload));
  EXPECT_TRUE(GetSessionContext()->did_transfer_wifi());
  histogram_tester_.ExpectBucketCount(kWifiTransferResultHistogramName, true,
                                      1);
}

TEST_F(TargetDeviceBootstrapControllerTest,
       RequestWifiCredentials_FailsIfNoResult) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  fake_target_device_connection_broker_->AuthenticateConnection(
      kSourceDeviceId, QuickStartMetrics::AuthenticationMethod::kQRCode);

  fake_target_device_connection_broker_->GetFakeConnection()->VerifyUser(
      mojom::UserVerificationResponse(
          mojom::UserVerificationResult::kUserVerified,
          /*is_first_user_verification=*/true));

  bootstrap_controller_->AttemptWifiCredentialTransfer();
  fake_target_device_connection_broker_->GetFakeConnection()
      ->SendWifiCredentials(std::nullopt);

  EXPECT_EQ(fake_observer_->last_status.step,
            Step::EMPTY_WIFI_CREDENTIALS_RECEIVED);
}

TEST_F(TargetDeviceBootstrapControllerTest, ConnectionFailsIfUserNotVerified) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  fake_target_device_connection_broker_->AuthenticateConnection(
      kSourceDeviceId, QuickStartMetrics::AuthenticationMethod::kQRCode);

  EXPECT_EQ(fake_observer_->last_status.step, Step::ADVERTISING_WITH_QR_CODE);
  EXPECT_TRUE(
      absl::holds_alternative<QRCode>(fake_observer_->last_status.payload));

  fake_target_device_connection_broker_->GetFakeConnection()->VerifyUser(
      mojom::UserVerificationResponse(
          mojom::UserVerificationResult::kUserNotVerified,
          /*is_first_user_verification=*/true));

  EXPECT_EQ(fake_observer_->last_status.step, Step::ERROR);
  EXPECT_EQ(absl::get<ErrorCode>(fake_observer_->last_status.payload),
            ErrorCode::USER_VERIFICATION_FAILED);
}

TEST_F(TargetDeviceBootstrapControllerTest,
       ConnectionFailsIfEmptyVerificationResult) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  fake_target_device_connection_broker_->AuthenticateConnection(
      kSourceDeviceId, QuickStartMetrics::AuthenticationMethod::kQRCode);

  EXPECT_EQ(fake_observer_->last_status.step, Step::ADVERTISING_WITH_QR_CODE);
  EXPECT_TRUE(
      absl::holds_alternative<QRCode>(fake_observer_->last_status.payload));

  fake_target_device_connection_broker_->GetFakeConnection()->VerifyUser(
      std::nullopt);

  EXPECT_EQ(fake_observer_->last_status.step, Step::ERROR);
  EXPECT_EQ(absl::get<ErrorCode>(fake_observer_->last_status.payload),
            ErrorCode::USER_VERIFICATION_FAILED);
}

TEST_F(TargetDeviceBootstrapControllerTest,
       RequestGoogleAccountInfoStateUpdates) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  fake_target_device_connection_broker_->AuthenticateConnection(
      kSourceDeviceId, QuickStartMetrics::AuthenticationMethod::kQRCode);

  bootstrap_controller_->RequestGoogleAccountInfo();

  EXPECT_EQ(fake_observer_->last_status.step,
            Step::REQUESTING_GOOGLE_ACCOUNT_INFO);
  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(
      fake_observer_->last_status.payload));

  std::string email = "fake_test_email";
  fake_target_device_connection_broker_->GetFakeConnection()->SendAccountInfo(
      email);

  EXPECT_EQ(fake_observer_->last_status.step,
            Step::GOOGLE_ACCOUNT_INFO_RECEIVED);
  EXPECT_EQ(*absl::get<EmailString>(fake_observer_->last_status.payload),
            email);
}

TEST_F(TargetDeviceBootstrapControllerTest,
       TransferringGaiaAccountSendsChallengeBytesToAuthenticatedConnection) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  fake_target_device_connection_broker_->AuthenticateConnection(
      kSourceDeviceId, QuickStartMetrics::AuthenticationMethod::kQRCode);

  auth_broker_->SetupChallengeBytesResponse(kFakeChallengeBytes_);
  bootstrap_controller_->AttemptGoogleAccountTransfer();

  EXPECT_EQ(fake_observer_->last_status.step,
            Step::TRANSFERRING_GOOGLE_ACCOUNT_DETAILS);
  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(
      fake_observer_->last_status.payload));

  EXPECT_EQ(fake_target_device_connection_broker_->GetFakeConnection()
                ->get_challenge(),
            kFakeChallengeBytes_);
}

TEST_F(TargetDeviceBootstrapControllerTest,
       FailureFetchingChallengeBytesIsProperlySurfaced) {
  histogram_tester_.ExpectBucketCount(kGaiaTransferResultHistogramName, false,
                                      0);
  histogram_tester_.ExpectBucketCount(
      kGaiaTransferResultFailureReasonHistogramName,
      QuickStartMetrics::GaiaTransferResultFailureReason::
          kFailedFetchingChallengeBytesFromGaia,
      0);
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  fake_target_device_connection_broker_->AuthenticateConnection(
      kSourceDeviceId, QuickStartMetrics::AuthenticationMethod::kQRCode);

  // Set up generic error as response
  auth_broker_->SetupChallengeBytesResponse(base::unexpected(
      GoogleServiceAuthError::FromServiceError("Unexpected Error")));
  bootstrap_controller_->AttemptGoogleAccountTransfer();

  EXPECT_EQ(fake_observer_->last_status.step, Step::ERROR);
  ASSERT_TRUE(
      absl::holds_alternative<ErrorCode>(fake_observer_->last_status.payload));
  EXPECT_EQ(absl::get<ErrorCode>(fake_observer_->last_status.payload),
            ErrorCode::FETCHING_CHALLENGE_BYTES_FAILED);
  histogram_tester_.ExpectBucketCount(kGaiaTransferResultHistogramName, false,
                                      1);
  histogram_tester_.ExpectBucketCount(
      kGaiaTransferResultFailureReasonHistogramName,
      QuickStartMetrics::GaiaTransferResultFailureReason::
          kFailedFetchingChallengeBytesFromGaia,
      1);
}

TEST_F(TargetDeviceBootstrapControllerTest,
       TransferGaiaAccountDetailsSucceeds) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  fake_target_device_connection_broker_->AuthenticateConnection(
      kSourceDeviceId, QuickStartMetrics::AuthenticationMethod::kQRCode);

  // Objects that will be used for verifying the data flow between the
  // components.
  FidoAssertionInfo fido_assertion;
  fido_assertion.credential_id = Base64UrlEncode(kTestCredentialId);
  PEMCertChain pem_cert_chain{kPemCertificateString};

  const auto auth_code_response =
      SecondDeviceAuthBroker::AuthCodeSuccessResponse{.auth_code =
                                                          kTestAuthCode};

  // TODO(b/287006890) - Expand test to include failure modes as well.
  auth_broker_->SetupChallengeBytesResponse(kFakeChallengeBytes_);
  auth_broker_->SetupAttestationCertificateResponse(pem_cert_chain);
  auth_broker_->SetupAuthCodeResponse(auth_code_response);

  bootstrap_controller_->AttemptGoogleAccountTransfer();

  EXPECT_EQ(fake_observer_->last_status.step,
            Step::TRANSFERRING_GOOGLE_ACCOUNT_DETAILS);
  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(
      fake_observer_->last_status.payload));

  // Expect that the credential_id of the FidoAssertion coming from
  // TargetDeviceConnectionBroker is correctly propagated into
  // SecondDeviceAuthBroker.
  EXPECT_CALL(*auth_broker_, FetchAttestationCertificate(
                                 fido_assertion.credential_id, testing::_))
      .Times(1);

  // Expect that TargetDeviceBootstrapController passes the certificate it
  // received from SecondDeviceAuthBroker back to it.
  EXPECT_CALL(*auth_broker_,
              FetchAuthCode(fido_assertion, pem_cert_chain, testing::_))
      .Times(1);

  fake_target_device_connection_broker_->GetFakeConnection()
      ->SendAccountTransferAssertionInfo(fido_assertion);

  EXPECT_EQ(fake_observer_->last_status.step,
            Step::TRANSFERRED_GOOGLE_ACCOUNT_DETAILS);
  const auto payload = fake_observer_->last_status.payload;
  EXPECT_TRUE(
      absl::holds_alternative<TargetDeviceBootstrapController::GaiaCredentials>(
          payload));
  const auto gaia_creds =
      absl::get<TargetDeviceBootstrapController::GaiaCredentials>(payload);
  EXPECT_EQ(gaia_creds.auth_code, kTestAuthCode);
}

TEST_F(TargetDeviceBootstrapControllerTest,
       TransferGaiaAccountDetailsFailsIfEmpty) {
  histogram_tester_.ExpectBucketCount(kGaiaTransferResultHistogramName, false,
                                      0);
  histogram_tester_.ExpectBucketCount(
      kGaiaTransferResultFailureReasonHistogramName,
      QuickStartMetrics::GaiaTransferResultFailureReason::
          kGaiaAssertionNotReceived,
      0);
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  fake_target_device_connection_broker_->AuthenticateConnection(
      kSourceDeviceId, QuickStartMetrics::AuthenticationMethod::kQRCode);

  auth_broker_->SetupChallengeBytesResponse(kFakeChallengeBytes_);
  bootstrap_controller_->AttemptGoogleAccountTransfer();

  EXPECT_EQ(fake_observer_->last_status.step,
            Step::TRANSFERRING_GOOGLE_ACCOUNT_DETAILS);
  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(
      fake_observer_->last_status.payload));

  fake_target_device_connection_broker_->GetFakeConnection()
      ->SendAccountTransferAssertionInfo(std::nullopt);

  EXPECT_EQ(fake_observer_->last_status.step, Step::ERROR);
  EXPECT_EQ(absl::get<ErrorCode>(fake_observer_->last_status.payload),
            ErrorCode::GAIA_ASSERTION_NOT_RECEIVED);
  histogram_tester_.ExpectBucketCount(kGaiaTransferResultHistogramName, false,
                                      1);
  histogram_tester_.ExpectBucketCount(
      kGaiaTransferResultFailureReasonHistogramName,
      QuickStartMetrics::GaiaTransferResultFailureReason::
          kGaiaAssertionNotReceived,
      1);
}

TEST_F(TargetDeviceBootstrapControllerTest, ConnectionDropped) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  fake_target_device_connection_broker_->AuthenticateConnection(
      kSourceDeviceId, QuickStartMetrics::AuthenticationMethod::kQRCode);

  EXPECT_EQ(fake_observer_->last_status.step, Step::ADVERTISING_WITH_QR_CODE);
  EXPECT_TRUE(
      absl::holds_alternative<QRCode>(fake_observer_->last_status.payload));

  bootstrap_controller_->StopAdvertising();
  fake_target_device_connection_broker_->on_stop_advertising_callback().Run();

  fake_target_device_connection_broker_->CloseConnection(
      ConnectionClosedReason::kUnknownError);

  EXPECT_EQ(fake_observer_->last_status.step, Step::ERROR);
  ASSERT_TRUE(
      absl::holds_alternative<ErrorCode>(fake_observer_->last_status.payload));
  EXPECT_EQ(absl::get<ErrorCode>(fake_observer_->last_status.payload),
            ErrorCode::CONNECTION_CLOSED);

  ExpectQuickStartConnectivityServiceCleanupCalled();
}

TEST_F(TargetDeviceBootstrapControllerTest, SessionContext) {
  // The SessionContext is generated by the bootstrap controller. Ensure that
  // the expected data is present when local state prefs indicate Quick
  // Start is resuming after an update.
  GetSessionContext()->FillOrResetSession();
  SessionContext::SessionId expected_session_id =
      GetSessionContext()->session_id();
  std::string expected_advertising_id =
      GetSessionContext()->advertising_id().ToString();
  SessionContext::SharedSecret expected_shared_secret =
      GetSessionContext()->secondary_shared_secret();

  ResumeAfterUpdate();
  GetSessionContext()->FillOrResetSession();

  EXPECT_TRUE(GetSessionContext()->is_resume_after_update());
  EXPECT_EQ(expected_session_id, GetSessionContext()->session_id());
  EXPECT_EQ(expected_advertising_id,
            GetSessionContext()->advertising_id().ToString());
  EXPECT_EQ(expected_shared_secret, GetSessionContext()->shared_secret());
}

TEST_F(TargetDeviceBootstrapControllerTest,
       ObserversAreNotNotifiedIfStatusStepIsSame) {
  EXPECT_EQ(0, fake_observer_->num_on_status_changed_called);
  UpdateStatus(/*step=*/Step::REQUESTING_WIFI_CREDENTIALS,
               /*payload=*/absl::monostate());
  EXPECT_EQ(1, fake_observer_->num_on_status_changed_called);

  // Updating status again with the same step shouldn't notify observers.
  UpdateStatus(/*step=*/Step::REQUESTING_WIFI_CREDENTIALS,
               /*payload=*/absl::monostate());
  EXPECT_EQ(1, fake_observer_->num_on_status_changed_called);
}

TEST_F(TargetDeviceBootstrapControllerTest,
       NoUserVerificationRequiredWhenResumeAfterUpdate) {
  GetSessionContext()->FillOrResetSession();
  ResumeAfterUpdate();
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  EXPECT_EQ(fake_observer_->last_status.step,
            Step::ADVERTISING_WITHOUT_QR_CODE);
  EXPECT_FALSE(
      absl::holds_alternative<QRCode>(fake_observer_->last_status.payload));
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  fake_target_device_connection_broker_->AuthenticateConnection(
      kSourceDeviceId,
      QuickStartMetrics::AuthenticationMethod::kResumeAfterUpdate);
  EXPECT_EQ(fake_observer_->last_status.step, Step::CONNECTED);
}

}  // namespace ash::quick_start
