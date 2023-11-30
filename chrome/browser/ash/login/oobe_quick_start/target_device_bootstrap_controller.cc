// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/account_transfer_client_data.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/qr_code.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/session_context.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker_factory.h"
#include "chrome/browser/ash/login/oobe_quick_start/oobe_quick_start_pref_names.h"
#include "chrome/browser/ash/login/oobe_quick_start/second_device_auth_broker.h"
#include "chrome/browser/ash/nearby/quick_start_connectivity_service.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/quick_start/logging.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
#include "chromeos/ash/components/quick_start/types.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/chromeos/devicetype_utils.h"
#include "url/origin.h"

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
    QuickStartConnectivityService* quick_start_connectivity_service)
    : auth_broker_(std::move(auth_broker)),
      accessibility_manager_wrapper_(std::move(accessibility_manager_wrapper)),
      quick_start_connectivity_service_(quick_start_connectivity_service) {
  connection_broker_ = TargetDeviceConnectionBrokerFactory::Create(
      session_context_, quick_start_connectivity_service_);
}

TargetDeviceBootstrapController::~TargetDeviceBootstrapController() {
  StopAdvertising();
  CloseOpenConnections(
      ConnectionClosedReason::kConnectionLifecycleListenerDestroyed);
  quick_start_connectivity_service_->Cleanup();
}

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

  bool use_pin_authentication =
      accessibility_manager_wrapper_->IsSpokenFeedbackEnabled();

  if (use_pin_authentication || session_context_.is_resume_after_update()) {
    status_.step = Step::ADVERTISING_WITHOUT_QR_CODE;
  } else {
    auto qr_code = std::make_unique<QRCode>(session_context_.advertising_id(),
                                            session_context_.shared_secret());
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

void TargetDeviceBootstrapController::CloseOpenConnections(
    ConnectionClosedReason reason) {
  // Close any existing open connection.
  if (authenticated_connection_.MaybeValid()) {
    authenticated_connection_->Close(reason);
    authenticated_connection_.reset();
  }

  CleanupIfNeeded();
}

void TargetDeviceBootstrapController::PrepareForUpdate() {
  if (status_.step != Step::WIFI_CREDENTIALS_RECEIVED ||
      !authenticated_connection_) {
    return;
  }

  authenticated_connection_->NotifySourceOfUpdate(base::BindOnce(
      &TargetDeviceBootstrapController::OnNotifySourceOfUpdateResponse,
      weak_ptr_factory_.GetWeakPtr()));
}

void TargetDeviceBootstrapController::OnPinVerificationRequested(
    const std::string& pin) {
  constexpr Step kPossibleSteps[] = {Step::ADVERTISING_WITHOUT_QR_CODE,
                                     Step::ADVERTISING_WITH_QR_CODE};
  CHECK(base::Contains(kPossibleSteps, status_.step));

  UpdateStatus(/*step=*/Step::PIN_VERIFICATION, /*payload=*/pin);
}

void TargetDeviceBootstrapController::OnConnectionAuthenticated(
    base::WeakPtr<TargetDeviceConnectionBroker::AuthenticatedConnection>
        authenticated_connection) {
  constexpr Step kPossibleSteps[] = {Step::ADVERTISING_WITH_QR_CODE,
                                     Step::ADVERTISING_WITHOUT_QR_CODE,
                                     Step::PIN_VERIFICATION};
  CHECK(base::Contains(kPossibleSteps, status_.step));
  authenticated_connection_ = authenticated_connection;
  WaitForUserVerification();
}

void TargetDeviceBootstrapController::OnConnectionRejected() {
  UpdateStatus(/*step=*/Step::ERROR,
               /*payload=*/ErrorCode::CONNECTION_REJECTED);
  CleanupIfNeeded();
}

void TargetDeviceBootstrapController::OnConnectionClosed(
    TargetDeviceConnectionBroker::ConnectionClosedReason reason) {
  if (status_.step == Step::REQUESTING_WIFI_CREDENTIALS) {
    QuickStartMetrics::RecordWifiTransferResult(
        /*succeeded=*/false, /*failure_reason=*/QuickStartMetrics::
            WifiTransferResultFailureReason::kConnectionDroppedDuringAttempt);
  }

  UpdateStatus(/*step=*/Step::ERROR, /*payload=*/ErrorCode::CONNECTION_CLOSED);
  authenticated_connection_.reset();
  CleanupIfNeeded();
}

std::string TargetDeviceBootstrapController::GetDiscoverableName() {
  std::string device_type = base::UTF16ToUTF8(ui::GetChromeOSDeviceName());
  std::string code = connection_broker_->GetAdvertisingIdDisplayCode();
  return device_type + " (" + code + ")";
}

void TargetDeviceBootstrapController::UpdateStatus(Step step, Payload payload) {
  if (status_.step == step) {
    return;
  }

  status_.step = step;
  status_.payload = payload;
  NotifyObservers();
}

void TargetDeviceBootstrapController::NotifyObservers() {
  QS_LOG(INFO)
      << "Notifying observers that the status has changed. New status step: "
      << status_.step;
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
  UpdateStatus(/*step=*/Step::ERROR,
               /*payload=*/ErrorCode::START_ADVERTISING_FAILED);
  CleanupIfNeeded();
}

void TargetDeviceBootstrapController::OnStopAdvertising() {
  // If we are just in the advertising stage, set status back to NONE to
  // indicate we are no longer performing any work. By contrast, if we are
  // connected or we got here due to an error, we don't want to update the
  // status.
  if (status_.step == Step::ADVERTISING_WITH_QR_CODE ||
      status_.step == Step::ADVERTISING_WITHOUT_QR_CODE) {
    UpdateStatus(/*step=*/Step::NONE, /*payload=*/absl::monostate());
  }

  CleanupIfNeeded();
}

void TargetDeviceBootstrapController::OnNotifySourceOfUpdateResponse(
    bool ack_successful) {
  CHECK(authenticated_connection_);

  if (ack_successful) {
    QS_LOG(INFO) << "Update ack sucessfully received. Preparing to resume "
                    "Quick Start after the update.";
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetBoolean(prefs::kShouldResumeQuickStartAfterReboot, true);
    base::Value::Dict info =
        authenticated_connection_->GetPrepareForUpdateInfo();
    prefs->SetDict(prefs::kResumeQuickStartAfterRebootInfo, std::move(info));
    prefs->CommitPendingWrite();
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

void TargetDeviceBootstrapController::WaitForUserVerification() {
  authenticated_connection_->WaitForUserVerification(
      base::BindOnce(&TargetDeviceBootstrapController::OnUserVerificationResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TargetDeviceBootstrapController::OnUserVerificationResult(
    absl::optional<mojom::UserVerificationResponse>
        user_verification_response) {
  if (!user_verification_response.has_value() ||
      user_verification_response->result ==
          mojom::UserVerificationResult::kUserNotVerified) {
    UpdateStatus(/*step=*/Step::ERROR,
                 /*payload=*/ErrorCode::USER_VERIFICATION_FAILED);
    return;
  }

  UpdateStatus(/*step=*/Step::CONNECTED, /*payload=*/absl::monostate());
}

void TargetDeviceBootstrapController::AttemptWifiCredentialTransfer() {
  UpdateStatus(/*step=*/Step::REQUESTING_WIFI_CREDENTIALS,
               /*payload=*/absl::monostate());

  authenticated_connection_->RequestWifiCredentials(base::BindOnce(
      &TargetDeviceBootstrapController::OnWifiCredentialsReceived,
      weak_ptr_factory_.GetWeakPtr()));
}

void TargetDeviceBootstrapController::OnWifiCredentialsReceived(
    absl::optional<mojom::WifiCredentials> credentials) {
  CHECK_EQ(status_.step, Step::REQUESTING_WIFI_CREDENTIALS);

  if (credentials.has_value()) {
    UpdateStatus(/*step=*/Step::WIFI_CREDENTIALS_RECEIVED,
                 /*payload=*/credentials.value());
  } else {
    UpdateStatus(/*step=*/Step::EMPTY_WIFI_CREDENTIALS_RECEIVED,
                 /*payload=*/absl::monostate());
  }

  // Record successful wifi credentials transfer. Failures will be
  // logged from the QuickStartDecoder class.
  QuickStartMetrics::RecordWifiTransferResult(
      /*succeeded=*/true, /*failure_reason=*/absl::nullopt);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kQuickStartTestForcedUpdateSwitch)) {
    PrepareForUpdate();
  }
}

void TargetDeviceBootstrapController::RequestGoogleAccountInfo() {
  CHECK(authenticated_connection_);

  UpdateStatus(/*step=*/Step::REQUESTING_GOOGLE_ACCOUNT_INFO,
               /*payload=*/absl::monostate());

  authenticated_connection_->RequestAccountInfo(base::BindOnce(
      &TargetDeviceBootstrapController::OnGoogleAccountInfoReceived,
      weak_ptr_factory_.GetWeakPtr()));
}

void TargetDeviceBootstrapController::OnGoogleAccountInfoReceived(
    std::string account_email) {
  UpdateStatus(/*step=*/Step::GOOGLE_ACCOUNT_INFO_RECEIVED,
               /*payload=*/account_email);
}

void TargetDeviceBootstrapController::AttemptGoogleAccountTransfer() {
  CHECK(authenticated_connection_);

  UpdateStatus(/*step=*/Step::TRANSFERRING_GOOGLE_ACCOUNT_DETAILS,
               /*payload=*/absl::monostate());

  // Request the challenge bytes from Gaia to be sent to the phone.
  CHECK(auth_broker_);
  auth_broker_->FetchChallengeBytes(
      base::BindOnce(&TargetDeviceBootstrapController::OnChallengeBytesReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TargetDeviceBootstrapController::Cleanup() {
  status_ = Status();
  CleanupIfNeeded();
}

void TargetDeviceBootstrapController::OnChallengeBytesReceived(
    SecondDeviceAuthBroker::ChallengeBytesOrError challenge) {
  if (!challenge.has_value()) {
    quick_start::QS_LOG(ERROR) << "Error fetching challenge bytes from Gaia. "
                               << "Reason: " << challenge.error().ToString();
    UpdateStatus(/*step=*/Step::ERROR,
                 /*payload=*/ErrorCode::FETCHING_CHALLENGE_BYTES_FAILED);
    QuickStartMetrics::RecordGaiaTransferAttempted(/*attempted=*/false);
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

  QuickStartMetrics::RecordGaiaTransferAttempted(/*attempted=*/true);
  authenticated_connection_->RequestAccountTransferAssertion(
      challenge_bytes_,
      base::BindOnce(&TargetDeviceBootstrapController::OnFidoAssertionReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TargetDeviceBootstrapController::OnFidoAssertionReceived(
    absl::optional<FidoAssertionInfo> assertion) {
  if (!assertion.has_value()) {
    UpdateStatus(/*step=*/Step::ERROR,
                 /*payload=*/ErrorCode::GAIA_ASSERTION_NOT_RECEIVED);
    return;
  }

  quick_start::QS_LOG(INFO) << "Received FIDO assertion.";
  fido_assertion_ = assertion.value();

  auth_broker_->FetchAttestationCertificate(
      fido_assertion_.credential_id,
      base::BindOnce(
          &TargetDeviceBootstrapController::OnAttestationCertificateReceived,
          weak_ptr_factory_.GetWeakPtr()));
}

void TargetDeviceBootstrapController::CleanupIfNeeded() {
  constexpr Step kPossibleSteps[] = {Step::NONE, Step::ERROR};
  if (base::Contains(kPossibleSteps, status_.step)) {
    quick_start_connectivity_service_->Cleanup();
  }
}

void TargetDeviceBootstrapController::OnAttestationCertificateReceived(
    quick_start::SecondDeviceAuthBroker::AttestationCertificateOrError
        attestation_certificate) {
  if (!attestation_certificate.has_value()) {
    // TODO(b/287006890) - Implement retry logic.
    quick_start::QS_LOG(ERROR) << "Error fetching attestation certificate. "
                               << "Reason: " << attestation_certificate.error();
    UpdateStatus(
        /*step=*/Step::ERROR,
        /*payload=*/ErrorCode::FETCHING_ATTESTATION_CERTIFICATE_FAILED);
    return;
  }

  quick_start::QS_LOG(INFO) << "Successfully retrieved attestation certificate";
  auth_broker_->FetchAuthCode(
      fido_assertion_, *attestation_certificate,
      base::BindOnce(&TargetDeviceBootstrapController::OnAuthCodeReceived,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TargetDeviceBootstrapController::OnAuthCodeReceived(
    const quick_start::SecondDeviceAuthBroker::AuthCodeResponse& response) {
  bool is_error = true;

  absl::visit(
      base::Overloaded{
          [&](SecondDeviceAuthBroker::AuthCodeSuccessResponse res) {
            quick_start::QS_LOG(INFO) << "Successfully fetched refresh token ";
            // TODO(b/287006890) Replace with auth code.
            UpdateStatus(/*step=*/Step::TRANSFERRED_GOOGLE_ACCOUNT_DETAILS,
                         /*payload=*/fido_assertion_);
            is_error = false;
          },
          [](SecondDeviceAuthBroker::
                 AuthCodeAdditionalChallengesOnTargetResponse res) {
            // TODO(b/310937296) - Implement fallback logic.
            quick_start::QS_LOG(ERROR)
                << "Failed to fetch refresh token! "
                   "Additional challenges needed on target device.";
          },
          [](SecondDeviceAuthBroker::
                 AuthCodeAdditionalChallengesOnSourceResponse res) {
            quick_start::QS_LOG(ERROR)
                << "Failed to fetch refresh token! "
                   "Additional challenges needed on source device.";
          },
          [](SecondDeviceAuthBroker::AuthCodeRejectionResponse res) {
            quick_start::QS_LOG(ERROR)
                << "Failed to fetch refresh token! Rejected: " << res.reason;
          },
          [](SecondDeviceAuthBroker::AuthCodeParsingErrorResponse res) {
            quick_start::QS_LOG(ERROR)
                << "Failed to fetch refresh token! Parsing error";
          },
          [](SecondDeviceAuthBroker::AuthCodeUnknownErrorResponse res) {
            quick_start::QS_LOG(ERROR)
                << "Failed to fetch refresh token! Unknown error";
          }},
      response);
  if (is_error) {
    UpdateStatus(/*step=*/Step::ERROR,
                 /*payload=*/ErrorCode::FETCHING_REFRESH_TOKEN_FAILED);
  }
}

std::ostream& operator<<(std::ostream& stream,
                         const TargetDeviceBootstrapController::Step& step) {
  switch (step) {
    case TargetDeviceBootstrapController::Step::NONE:
      stream << "[none]";
      break;
    case TargetDeviceBootstrapController::Step::ERROR:
      stream << "[error]";
      break;
    case TargetDeviceBootstrapController::Step::ADVERTISING_WITH_QR_CODE:
      stream << "[advertising with QR code]";
      break;
    case TargetDeviceBootstrapController::Step::ADVERTISING_WITHOUT_QR_CODE:
      stream << "[advertising without QR code]";
      break;
    case TargetDeviceBootstrapController::Step::PIN_VERIFICATION:
      stream << "[pin verification]";
      break;
    case TargetDeviceBootstrapController::Step::CONNECTED:
      stream << "[connected]";
      break;
    case TargetDeviceBootstrapController::Step::REQUESTING_WIFI_CREDENTIALS:
      stream << "[requesting wifi credentials]";
      break;
    case TargetDeviceBootstrapController::Step::WIFI_CREDENTIALS_RECEIVED:
      stream << "[wifi credentials received]";
      break;
    case TargetDeviceBootstrapController::Step::EMPTY_WIFI_CREDENTIALS_RECEIVED:
      stream << "[empty wifi credentials received]";
      break;
    case TargetDeviceBootstrapController::Step::REQUESTING_GOOGLE_ACCOUNT_INFO:
      stream << "[requesting google account info]";
      break;
    case TargetDeviceBootstrapController::Step::GOOGLE_ACCOUNT_INFO_RECEIVED:
      stream << "[google account info received]";
      break;
    case TargetDeviceBootstrapController::Step::
        TRANSFERRING_GOOGLE_ACCOUNT_DETAILS:
      stream << "[transferring Google account details]";
      break;
    case TargetDeviceBootstrapController::Step::
        TRANSFERRED_GOOGLE_ACCOUNT_DETAILS:
      stream << "[transferred Google account details]";
      break;
  }

  return stream;
}

std::ostream& operator<<(
    std::ostream& stream,
    const TargetDeviceBootstrapController::ErrorCode& error_code) {
  switch (error_code) {
    case TargetDeviceBootstrapController::ErrorCode::START_ADVERTISING_FAILED:
      stream << "[start advertising failed]";
      break;
    case TargetDeviceBootstrapController::ErrorCode::CONNECTION_REJECTED:
      stream << "[connection rejected]";
      break;
    case TargetDeviceBootstrapController::ErrorCode::CONNECTION_CLOSED:
      stream << "[connection closed]";
      break;
    case TargetDeviceBootstrapController::ErrorCode::USER_VERIFICATION_FAILED:
      stream << "[user verification failed]";
      break;
    case TargetDeviceBootstrapController::ErrorCode::
        GAIA_ASSERTION_NOT_RECEIVED:
      stream << "[Gaia assertion not received]";
      break;
    case TargetDeviceBootstrapController::ErrorCode::
        FETCHING_CHALLENGE_BYTES_FAILED:
      stream << "[fetching Challenge Bytes failed]";
      break;
    case TargetDeviceBootstrapController::ErrorCode::
        FETCHING_ATTESTATION_CERTIFICATE_FAILED:
      stream << "[fetching attestation certificate failed]";
      break;
    case TargetDeviceBootstrapController::ErrorCode::
        FETCHING_REFRESH_TOKEN_FAILED:
      stream << "[fetching refresh token failed]";
      break;
  }

  return stream;
}

}  // namespace ash::quick_start
