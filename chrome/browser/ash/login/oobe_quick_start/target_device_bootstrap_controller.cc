// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/qr_code.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/session_context.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker_factory.h"
#include "chrome/browser/ash/login/oobe_quick_start/oobe_quick_start_pref_names.h"
#include "chrome/browser/ash/login/oobe_quick_start/second_device_auth_broker.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/quick_start/logging.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash::quick_start {

namespace {

// Passing "--quick-start-test-forced-update" on the command line will simulate
// the "Forced Update" flow after the wifi credentials transfer is complete.
// This is for testing only and will not install an actual update. If this
// switch is present, the Chromebook reboots and attempts to automatically
// resume the Quick Start connection after reboot.
// TODO(b/280308144): Delete this switch. The OOBE update screen should call
// PrepareForUpdate() and trigger the update/reboot.
constexpr char kQuickStartTestForcedUpdateSwitch[] =
    "quick-start-test-forced-update";

}  // namespace

TargetDeviceBootstrapController::TargetDeviceBootstrapController(
    std::unique_ptr<SecondDeviceAuthBroker> auth_broker,
    std::unique_ptr<
        TargetDeviceBootstrapController::AccessibilityManagerWrapper>
        accessibility_manager_wrapper,
    base::WeakPtr<NearbyConnectionsManager> nearby_connections_manager,
    mojo::SharedRemote<mojom::QuickStartDecoder> quick_start_decoder)
    : auth_broker_(std::move(auth_broker)),
      accessibility_manager_wrapper_(std::move(accessibility_manager_wrapper)) {
  session_context_ = SessionContext();
  connection_broker_ = TargetDeviceConnectionBrokerFactory::Create(
      session_context_, nearby_connections_manager, quick_start_decoder);
}

TargetDeviceBootstrapController::~TargetDeviceBootstrapController() = default;

TargetDeviceBootstrapController::Status::Status() = default;
TargetDeviceBootstrapController::Status::~Status() = default;

void TargetDeviceBootstrapController::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void TargetDeviceBootstrapController::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void TargetDeviceBootstrapController::GetFeatureSupportStatusAsync(
    TargetDeviceConnectionBroker::FeatureSupportStatusCallback callback) {
  connection_broker_->GetFeatureSupportStatusAsync(std::move(callback));
}

std::string TargetDeviceBootstrapController::GetPhoneInstanceId() {
  return authenticated_connection_->get_phone_instance_id();
}

base::WeakPtr<TargetDeviceBootstrapController>
TargetDeviceBootstrapController::GetAsWeakPtrForClient() {
  // Only one client at a time should have a pointer.
  DCHECK(!weak_ptr_factory_for_clients_.HasWeakPtrs());
  return weak_ptr_factory_for_clients_.GetWeakPtr();
}

void TargetDeviceBootstrapController::StartAdvertisingAndMaybeGetQRCode() {
  CHECK(connection_broker_->GetFeatureSupportStatus() ==
        TargetDeviceConnectionBroker::FeatureSupportStatus::kSupported);
  CHECK_EQ(status_.step, Step::NONE);

  // No pending requests.
  CHECK(!weak_ptr_factory_.HasWeakPtrs());

  bool use_pin_authentication =
      accessibility_manager_wrapper_->IsSpokenFeedbackEnabled();

  if (use_pin_authentication || session_context_.is_resume_after_update()) {
    status_.step = Step::ADVERTISING_WITHOUT_QR_CODE;
  } else {
    auto qr_code = std::make_unique<QRCode>(
        session_context_.random_session_id(), session_context_.shared_secret());
    status_.step = Step::ADVERTISING_WITH_QR_CODE;
    status_.payload.emplace<QRCode::PixelData>(qr_code->pixel_data());
  }

  connection_broker_->StartAdvertising(
      this, use_pin_authentication,
      base::BindOnce(&TargetDeviceBootstrapController::OnStartAdvertisingResult,
                     weak_ptr_factory_.GetWeakPtr()));
  NotifyObservers();
}

void TargetDeviceBootstrapController::StopAdvertising() {
  // Connection broker ignores the request if not advertising.
  connection_broker_->StopAdvertising(
      base::BindOnce(&TargetDeviceBootstrapController::OnStopAdvertising,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TargetDeviceBootstrapController::MaybeCloseOpenConnections() {
  // Close any existing open connection.
  if (authenticated_connection_.MaybeValid()) {
    authenticated_connection_->Close(
        TargetDeviceConnectionBroker::ConnectionClosedReason::kUserAborted);
  }
}

void TargetDeviceBootstrapController::PrepareForUpdate() {
  if (status_.step != Step::CONNECTED_TO_WIFI || !authenticated_connection_) {
    return;
  }

  authenticated_connection_->NotifySourceOfUpdate(
      session_id_,
      base::BindOnce(
          &TargetDeviceBootstrapController::OnNotifySourceOfUpdateResponse,
          weak_ptr_factory_.GetWeakPtr()));
}

void TargetDeviceBootstrapController::OnPinVerificationRequested(
    const std::string& pin) {
  constexpr Step kPossibleSteps[] = {Step::ADVERTISING_WITHOUT_QR_CODE,
                                     Step::ADVERTISING_WITH_QR_CODE};
  CHECK(base::Contains(kPossibleSteps, status_.step));

  pin_ = pin;
  status_.step = Step::PIN_VERIFICATION;
  status_.pin = pin_;
  status_.payload.emplace<absl::monostate>();
  NotifyObservers();
}

void TargetDeviceBootstrapController::OnConnectionAuthenticated(
    base::WeakPtr<TargetDeviceConnectionBroker::AuthenticatedConnection>
        authenticated_connection) {
  constexpr Step kPossibleSteps[] = {Step::ADVERTISING_WITH_QR_CODE,
                                     Step::PIN_VERIFICATION};
  CHECK(base::Contains(kPossibleSteps, status_.step));

  authenticated_connection_ = authenticated_connection;

  // Create session ID by generating UUID and then hashing.
  const base::Uuid random_uuid = base::Uuid::GenerateRandomV4();
  session_id_ = static_cast<int32_t>(
      base::PersistentHash(random_uuid.AsLowercaseString()));

  status_.step = Step::CONNECTED;
  status_.payload.emplace<absl::monostate>();
  NotifyObservers();
  AttemptWifiCredentialTransfer();
}

void TargetDeviceBootstrapController::OnConnectionRejected() {
  status_.step = Step::ERROR;
  status_.payload = ErrorCode::CONNECTION_REJECTED;
  NotifyObservers();
}

void TargetDeviceBootstrapController::OnConnectionClosed(
    TargetDeviceConnectionBroker::ConnectionClosedReason reason) {
  if (status_.step == Step::CONNECTING_TO_WIFI) {
    quick_start_metrics::RecordWifiTransferResult(
        /*succeeded=*/false, /*failure_reason=*/quick_start_metrics::
            WifiTransferResultFailureReason::kConnectionDroppedDuringAttempt);
  }
  status_.step = Step::ERROR;
  status_.payload = ErrorCode::CONNECTION_CLOSED;
  authenticated_connection_.reset();
  NotifyObservers();
}

std::string TargetDeviceBootstrapController::GetDiscoverableName() {
  std::string device_type = base::UTF16ToUTF8(ui::GetChromeOSDeviceName());
  std::string code = connection_broker_->GetSessionIdDisplayCode();
  return device_type + " (" + code + ")";
}

void TargetDeviceBootstrapController::NotifyObservers() {
  for (auto& obs : observers_) {
    obs.OnStatusChanged(status_);
  }
}

void TargetDeviceBootstrapController::OnStartAdvertisingResult(bool success) {
  constexpr Step kPossibleSteps[] = {Step::ADVERTISING_WITH_QR_CODE,
                                     Step::ADVERTISING_WITHOUT_QR_CODE};
  CHECK(base::Contains(kPossibleSteps, status_.step));
  if (success) {
    return;
  }
  status_.step = Step::ERROR;
  status_.payload = ErrorCode::START_ADVERTISING_FAILED;
  NotifyObservers();
}

void TargetDeviceBootstrapController::OnStopAdvertising() {
  status_.step = Step::NONE;
  status_.payload.emplace<absl::monostate>();
  NotifyObservers();
}

void TargetDeviceBootstrapController::OnNotifySourceOfUpdateResponse(
    bool ack_successful) {
  CHECK(authenticated_connection_);

  if (ack_successful || base::CommandLine::ForCurrentProcess()->HasSwitch(
                            kQuickStartTestForcedUpdateSwitch)) {
    QS_LOG(INFO) << "Update ack sucessfully received. Preparing to resume "
                    "Quick Start after the update.";
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetBoolean(prefs::kShouldResumeQuickStartAfterReboot, true);
    base::Value::Dict info =
        authenticated_connection_->GetPrepareForUpdateInfo();
    prefs->SetDict(prefs::kResumeQuickStartAfterRebootInfo, std::move(info));
  }

  authenticated_connection_->Close(
      TargetDeviceConnectionBroker::ConnectionClosedReason::
          kTargetDeviceUpdate);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kQuickStartTestForcedUpdateSwitch)) {
    chromeos::PowerManagerClient::Get()->RequestRestart(
        power_manager::REQUEST_RESTART_FOR_UPDATE,
        "Testing OOBE Quick Start Forced Update flow");
  }
}

void TargetDeviceBootstrapController::WaitForUserVerification(
    base::OnceClosure on_verification) {
  authenticated_connection_->WaitForUserVerification(base::BindOnce(
      &TargetDeviceBootstrapController::OnUserVerificationResult,
      weak_ptr_factory_.GetWeakPtr(), std::move(on_verification)));
}

void TargetDeviceBootstrapController::OnUserVerificationResult(
    base::OnceClosure on_verification,
    absl::optional<mojom::UserVerificationResponse>
        user_verification_response) {
  if (!user_verification_response.has_value() ||
      user_verification_response->result ==
          mojom::UserVerificationResult::kUserNotVerified) {
    status_.step = Step::ERROR;
    status_.payload = ErrorCode::USER_VERIFICATION_FAILED;
    NotifyObservers();
    return;
  }

  std::move(on_verification).Run();
}

void TargetDeviceBootstrapController::AttemptWifiCredentialTransfer() {
  status_.step = Step::CONNECTING_TO_WIFI;
  status_.payload.emplace<absl::monostate>();

  WaitForUserVerification(base::BindOnce(
      &TargetDeviceConnectionBroker::AuthenticatedConnection::
          RequestWifiCredentials,
      authenticated_connection_, session_id_,
      base::BindOnce(
          &TargetDeviceBootstrapController::OnWifiCredentialsReceived,
          weak_ptr_factory_.GetWeakPtr())));

  NotifyObservers();
}

void TargetDeviceBootstrapController::OnWifiCredentialsReceived(
    absl::optional<mojom::WifiCredentials> credentials) {
  CHECK_EQ(status_.step, Step::CONNECTING_TO_WIFI);
  if (!credentials.has_value()) {
    status_.step = Step::ERROR;
    status_.payload = ErrorCode::WIFI_CREDENTIALS_NOT_RECEIVED;
    NotifyObservers();
    return;
  }

  status_.step = Step::CONNECTED_TO_WIFI;
  status_.payload.emplace<absl::monostate>();
  status_.ssid = credentials->ssid;
  status_.password = credentials->password;
  NotifyObservers();

  // Record successful wifi credentials transfer. Failures will be
  // logged from the QuickStartDecoder class.
  quick_start_metrics::RecordWifiTransferResult(
      /*succeeded=*/true, /*failure_reason=*/absl::nullopt);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kQuickStartTestForcedUpdateSwitch)) {
    PrepareForUpdate();
  }
}

void TargetDeviceBootstrapController::AttemptGoogleAccountTransfer() {
  CHECK(authenticated_connection_);

  status_.step = Step::TRANSFERRING_GOOGLE_ACCOUNT_DETAILS;
  status_.payload.emplace<absl::monostate>();
  NotifyObservers();

  // Request the challenge bytes from Gaia to be sent to the phone.
  CHECK(auth_broker_);
  auth_broker_->FetchChallengeBytes(
      base::BindOnce(&TargetDeviceBootstrapController::OnChallengeBytesReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TargetDeviceBootstrapController::OnChallengeBytesReceived(
    SecondDeviceAuthBroker::ChallengeBytesOrError challenge) {
  if (!challenge.has_value()) {
    quick_start::QS_LOG(ERROR) << "Error fetching challenge bytes from Gaia. "
                               << "Reason: " << challenge.error().ToString();
    status_.step = Step::ERROR;
    status_.payload = ErrorCode::FETCHING_CHALLENGE_BYTES_FAILED;
    quick_start_metrics::RecordGaiaTransferAttempted(/*attempted=*/false);
    NotifyObservers();
    return;
    // TODO(b:286853512) - Implement retry mechanism.
  }

  if (!authenticated_connection_) {
    quick_start::QS_LOG(ERROR)
        << "Received challenge bytes, but a phone connection no longer exists.";
    NOTIMPLEMENTED();
  }

  quick_start::QS_LOG(INFO)
      << "Received challenge bytes from Gaia. Requesting FIDO assertion.";
  challenge_bytes_ = challenge.value();

  quick_start_metrics::RecordGaiaTransferAttempted(/*attempted=*/true);
  authenticated_connection_->RequestAccountTransferAssertion(
      challenge_bytes_,
      base::BindOnce(&TargetDeviceBootstrapController::OnFidoAssertionReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TargetDeviceBootstrapController::OnFidoAssertionReceived(
    absl::optional<FidoAssertionInfo> assertion) {
  if (!assertion.has_value()) {
    status_.step = Step::ERROR;
    status_.payload = ErrorCode::GAIA_ASSERTION_NOT_RECEIVED;
    NotifyObservers();
    return;
  }

  status_.step = Step::TRANSFERRED_GOOGLE_ACCOUNT_DETAILS;
  status_.payload.emplace<FidoAssertionInfo>(assertion.value());
  NotifyObservers();
}

}  // namespace ash::quick_start
