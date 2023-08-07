// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"

#include <memory>
#include <string>

#include "base/base64.h"
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
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/mock_second_device_auth_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/oobe_quick_start_pref_names.h"
#include "chrome/browser/ash/login/oobe_quick_start/second_device_auth_broker.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connections_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/quick_start/fake_quick_start_decoder.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
#include "chromeos/ash/components/quick_start/types.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash::quick_start {

namespace {

using Observer = TargetDeviceBootstrapController::Observer;
using Status = TargetDeviceBootstrapController::Status;
using Step = TargetDeviceBootstrapController::Step;
using ErrorCode = TargetDeviceBootstrapController::ErrorCode;
using ConnectionClosedReason =
    TargetDeviceConnectionBroker::ConnectionClosedReason;

constexpr char kWifiTransferResultHistogramName[] =
    "QuickStart.WifiTransferResult";
constexpr char kWifiTransferResultFailureReasonHistogramName[] =
    "QuickStart.WifiTransferResult.FailureReason";
constexpr char kGaiaTransferAttemptedName[] =
    "QuickStart.GaiaTransferAttempted";

class FakeObserver : public Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  void OnStatusChanged(const Status& status) override {
    // Step must change.
    ASSERT_NE(status.step, last_status.step);

    last_status = status;
  }

  Status last_status;
};

constexpr char kFakeChallengeBytesBase64[] =
    "ABz12ClFhY8/D89zWFB+KTHgUwJ5T3Avco/1IQuu+K/"
    "65KlsmB7o0+UyPde8ZW+b33aeJ9uyST8EMzS6WhK60e/VDjug+7LLK4YzDz1nNw==";

class FakeAccessibilityManagerWrapper
    : public TargetDeviceBootstrapController::AccessibilityManagerWrapper {
 public:
  bool IsSpokenFeedbackEnabled() const override {
    return spoken_feedback_enabled_;
  }

  bool spoken_feedback_enabled_ = false;
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

    bootstrap_controller_ = std::make_unique<TargetDeviceBootstrapController>(
        std::move(auth_broker), std::move(fake_accessibility_manager),
        fake_nearby_connections_manager_.GetWeakPtr(),
        mojo::SharedRemote<mojom::QuickStartDecoder>(
            fake_quick_start_decoder_.GetRemote()));

    std::unique_ptr<FakeTargetDeviceConnectionBroker>
        fake_target_device_connection_broker =
            std::make_unique<FakeTargetDeviceConnectionBroker>();
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
        kSourceDeviceId);
    ASSERT_EQ(fake_observer_->last_status.step, Step::CONNECTING_TO_WIFI);
  }

  void NotifySourceOfUpdateResponse(bool ack_successful) {
    bootstrap_controller_->OnNotifySourceOfUpdateResponse(ack_successful);
  }

  PrefService* GetLocalState() { return local_state_.Get(); }

 protected:
  absl::optional<FidoAssertionInfo> assertion_info_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_factory_;
  raw_ptr<FakeTargetDeviceConnectionBroker, ExperimentalAsh>
      fake_target_device_connection_broker_;
  FakeNearbyConnectionsManager fake_nearby_connections_manager_;
  FakeQuickStartDecoder fake_quick_start_decoder_;
  std::unique_ptr<FakeObserver> fake_observer_;
  raw_ptr<MockAuthBroker> auth_broker_;
  raw_ptr<FakeAccessibilityManagerWrapper, ExperimentalAsh>
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
  EXPECT_TRUE(absl::holds_alternative<QRCode::PixelData>(
      fake_observer_->last_status.payload));
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
}

TEST_F(TargetDeviceBootstrapControllerTest,
       StartAdvertisingWithChromevoxUsesPin) {
  fake_accessibility_manager_->spoken_feedback_enabled_ = true;
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  EXPECT_EQ(fake_observer_->last_status.step,
            Step::ADVERTISING_WITHOUT_QR_CODE);
  EXPECT_FALSE(absl::holds_alternative<QRCode::PixelData>(
      fake_observer_->last_status.payload));
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
}

TEST_F(TargetDeviceBootstrapControllerTest, InitiateConnection_QRCode) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  ASSERT_EQ(fake_observer_->last_status.step, Step::ADVERTISING_WITH_QR_CODE);
  EXPECT_TRUE(absl::holds_alternative<QRCode::PixelData>(
      fake_observer_->last_status.payload));

  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  // Status shouldn't change.
  EXPECT_EQ(fake_observer_->last_status.step, Step::ADVERTISING_WITH_QR_CODE);
  EXPECT_TRUE(absl::holds_alternative<QRCode::PixelData>(
      fake_observer_->last_status.payload));
}

TEST_F(TargetDeviceBootstrapControllerTest, InitiateConnection_Pin) {
  fake_accessibility_manager_->spoken_feedback_enabled_ = true;
  fake_target_device_connection_broker_->set_use_pin_authentication(true);
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  ASSERT_EQ(fake_observer_->last_status.step,
            Step::ADVERTISING_WITHOUT_QR_CODE);

  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);

  EXPECT_EQ(fake_observer_->last_status.step, Step::PIN_VERIFICATION);
  // TODO: Test PIN payload
  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(
      fake_observer_->last_status.payload));
}

TEST_F(TargetDeviceBootstrapControllerTest, AuthenticateConnection) {
  BootstrapConnection();
  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(
      fake_observer_->last_status.payload));
}

TEST_F(TargetDeviceBootstrapControllerTest, FeatureSupportStatus) {
  absl::optional<TargetDeviceConnectionBroker::FeatureSupportStatus>
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
      ConnectionClosedReason::kConnectionLost);

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
      kSourceDeviceId);

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
      kSourceDeviceId);

  EXPECT_EQ(fake_observer_->last_status.step, Step::CONNECTING_TO_WIFI);
  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(
      fake_observer_->last_status.payload));

  fake_target_device_connection_broker_->GetFakeConnection()->VerifyUser(
      mojom::UserVerificationResponse(
          mojom::UserVerificationResult::kUserVerified,
          /*is_first_user_verification=*/true));

  fake_target_device_connection_broker_->GetFakeConnection()
      ->SendWifiCredentials(
          mojom::WifiCredentials("ssid", mojom::WifiSecurityType::kWEP,
                                 /*is_hidden=*/true, "password"));

  EXPECT_EQ(fake_observer_->last_status.step, Step::CONNECTED_TO_WIFI);
  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(
      fake_observer_->last_status.payload));
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
      kSourceDeviceId);

  fake_target_device_connection_broker_->GetFakeConnection()->VerifyUser(
      mojom::UserVerificationResponse(
          mojom::UserVerificationResult::kUserVerified,
          /*is_first_user_verification=*/true));

  fake_target_device_connection_broker_->GetFakeConnection()
      ->SendWifiCredentials(absl::nullopt);

  EXPECT_EQ(fake_observer_->last_status.step, Step::ERROR);
  EXPECT_EQ(absl::get<ErrorCode>(fake_observer_->last_status.payload),
            ErrorCode::WIFI_CREDENTIALS_NOT_RECEIVED);
}

TEST_F(TargetDeviceBootstrapControllerTest,
       RequestWifiCredentialsFailsIfUserNotVerified) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  fake_target_device_connection_broker_->AuthenticateConnection(
      kSourceDeviceId);

  EXPECT_EQ(fake_observer_->last_status.step, Step::CONNECTING_TO_WIFI);
  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(
      fake_observer_->last_status.payload));

  fake_target_device_connection_broker_->GetFakeConnection()->VerifyUser(
      mojom::UserVerificationResponse(
          mojom::UserVerificationResult::kUserNotVerified,
          /*is_first_user_verification=*/true));

  EXPECT_EQ(fake_observer_->last_status.step, Step::ERROR);
  EXPECT_EQ(absl::get<ErrorCode>(fake_observer_->last_status.payload),
            ErrorCode::USER_VERIFICATION_FAILED);
}

TEST_F(TargetDeviceBootstrapControllerTest,
       RequestWifiCredentialsFailsIfEmptyVerificationResult) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  fake_target_device_connection_broker_->AuthenticateConnection(
      kSourceDeviceId);

  EXPECT_EQ(fake_observer_->last_status.step, Step::CONNECTING_TO_WIFI);
  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(
      fake_observer_->last_status.payload));

  fake_target_device_connection_broker_->GetFakeConnection()->VerifyUser(
      absl::nullopt);

  EXPECT_EQ(fake_observer_->last_status.step, Step::ERROR);
  EXPECT_EQ(absl::get<ErrorCode>(fake_observer_->last_status.payload),
            ErrorCode::USER_VERIFICATION_FAILED);
}

TEST_F(TargetDeviceBootstrapControllerTest,
       TransferringGaiaAccountSendsChallengeBytesToAuthenticatedConnection) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  fake_target_device_connection_broker_->AuthenticateConnection(
      kSourceDeviceId);

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
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  fake_target_device_connection_broker_->AuthenticateConnection(
      kSourceDeviceId);

  // Set up generic error as response
  auth_broker_->SetupChallengeBytesResponse(base::unexpected(
      GoogleServiceAuthError::FromServiceError("Unexpected Error")));
  bootstrap_controller_->AttemptGoogleAccountTransfer();

  EXPECT_EQ(fake_observer_->last_status.step, Step::ERROR);
  ASSERT_TRUE(
      absl::holds_alternative<ErrorCode>(fake_observer_->last_status.payload));
  EXPECT_EQ(absl::get<ErrorCode>(fake_observer_->last_status.payload),
            ErrorCode::FETCHING_CHALLENGE_BYTES_FAILED);
  histogram_tester_.ExpectBucketCount(kGaiaTransferAttemptedName, false, 1);
}

TEST_F(TargetDeviceBootstrapControllerTest,
       TransferGaiaAccountDetailsSucceeds) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  fake_target_device_connection_broker_->AuthenticateConnection(
      kSourceDeviceId);

  auth_broker_->SetupChallengeBytesResponse(kFakeChallengeBytes_);
  bootstrap_controller_->AttemptGoogleAccountTransfer();

  EXPECT_EQ(fake_observer_->last_status.step,
            Step::TRANSFERRING_GOOGLE_ACCOUNT_DETAILS);
  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(
      fake_observer_->last_status.payload));

  fake_target_device_connection_broker_->GetFakeConnection()
      ->SendAccountTransferAssertionInfo(FidoAssertionInfo());

  EXPECT_EQ(fake_observer_->last_status.step,
            Step::TRANSFERRED_GOOGLE_ACCOUNT_DETAILS);
  EXPECT_TRUE(absl::holds_alternative<FidoAssertionInfo>(
      fake_observer_->last_status.payload));
  histogram_tester_.ExpectBucketCount(kGaiaTransferAttemptedName, true, 1);
}

TEST_F(TargetDeviceBootstrapControllerTest,
       TransferGaiaAccountDetailsFailsIfEmpty) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  fake_target_device_connection_broker_->AuthenticateConnection(
      kSourceDeviceId);

  auth_broker_->SetupChallengeBytesResponse(kFakeChallengeBytes_);
  bootstrap_controller_->AttemptGoogleAccountTransfer();

  EXPECT_EQ(fake_observer_->last_status.step,
            Step::TRANSFERRING_GOOGLE_ACCOUNT_DETAILS);
  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(
      fake_observer_->last_status.payload));

  fake_target_device_connection_broker_->GetFakeConnection()
      ->SendAccountTransferAssertionInfo(absl::nullopt);

  EXPECT_EQ(fake_observer_->last_status.step, Step::ERROR);
  EXPECT_EQ(absl::get<ErrorCode>(fake_observer_->last_status.payload),
            ErrorCode::GAIA_ASSERTION_NOT_RECEIVED);
  histogram_tester_.ExpectBucketCount(kGaiaTransferAttemptedName, true, 1);
}

// Ensures that the discoverable name that is shown Chromebook (123) matches
// the one returned by RandomSessionId
TEST_F(TargetDeviceBootstrapControllerTest, DiscoverableName) {
  std::string device_type = base::UTF16ToUTF8(ui::GetChromeOSDeviceName());
  std::string code =
      fake_target_device_connection_broker_->GetSessionIdDisplayCode();
  auto expected_string = device_type + " (" + code + ")";

  EXPECT_EQ(bootstrap_controller_->GetDiscoverableName(), expected_string);
}

TEST_F(TargetDeviceBootstrapControllerTest,
       RequestWifiCredentials_ConnectionDropped) {
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
  fake_target_device_connection_broker_->on_start_advertising_callback().Run(
      /*success=*/true);
  fake_target_device_connection_broker_->InitiateConnection(kSourceDeviceId);
  fake_target_device_connection_broker_->AuthenticateConnection(
      kSourceDeviceId);

  EXPECT_EQ(fake_observer_->last_status.step, Step::CONNECTING_TO_WIFI);
  EXPECT_TRUE(absl::holds_alternative<absl::monostate>(
      fake_observer_->last_status.payload));

  fake_target_device_connection_broker_->CloseConnection(
      ConnectionClosedReason::kConnectionLost);

  EXPECT_EQ(fake_observer_->last_status.step, Step::ERROR);
  ASSERT_TRUE(
      absl::holds_alternative<ErrorCode>(fake_observer_->last_status.payload));
  EXPECT_EQ(absl::get<ErrorCode>(fake_observer_->last_status.payload),
            ErrorCode::CONNECTION_CLOSED);

  histogram_tester_.ExpectBucketCount(kWifiTransferResultHistogramName, false,
                                      1);
  histogram_tester_.ExpectBucketCount(
      kWifiTransferResultFailureReasonHistogramName,
      quick_start_metrics::WifiTransferResultFailureReason::
          kConnectionDroppedDuringAttempt,
      1);
}

}  // namespace ash::quick_start
