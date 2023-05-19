// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker_factory.h"
#include "chrome/browser/ash/login/oobe_quick_start/logging/logging.h"
#include "chrome/browser/ash/login/oobe_quick_start/oobe_quick_start_pref_names.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/qr_code_generator/qr_code_generator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace ash::quick_start {

namespace {

TargetDeviceBootstrapController::QRCodePixelData GenerateQRCode(
    std::vector<uint8_t> blob) {
  QRCodeGenerator qr_generator;
  auto generated_code = qr_generator.Generate(
      base::as_bytes(base::make_span(blob.data(), blob.size())));
  CHECK(generated_code.has_value());
  auto res = TargetDeviceBootstrapController::QRCodePixelData{
      generated_code->data.begin(), generated_code->data.end()};
  CHECK_EQ(res.size(), static_cast<size_t>(generated_code->qr_size *
                                           generated_code->qr_size));
  return res;
}

}  // namespace

TargetDeviceBootstrapController::TargetDeviceBootstrapController(
    std::unique_ptr<TargetDeviceConnectionBroker>
        target_device_connection_broker)
    : connection_broker_(std::move(target_device_connection_broker)) {}

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
  // TODO(b/234655072): Get the ID from the Gaia credentials exchange.
  return "";
}

base::WeakPtr<TargetDeviceBootstrapController>
TargetDeviceBootstrapController::GetAsWeakPtrForClient() {
  // Only one client at a time should have a pointer.
  DCHECK(!weak_ptr_factory_for_clients_.HasWeakPtrs());
  return weak_ptr_factory_for_clients_.GetWeakPtr();
}

void TargetDeviceBootstrapController::StartAdvertising() {
  DCHECK(connection_broker_->GetFeatureSupportStatus() ==
         TargetDeviceConnectionBroker::FeatureSupportStatus::kSupported);
  DCHECK_EQ(status_.step, Step::NONE);

  // No pending requests.
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());

  status_.step = Step::ADVERTISING;
  connection_broker_->StartAdvertising(
      this, /*use_pin_authentication=*/true,
      base::BindOnce(&TargetDeviceBootstrapController::OnStartAdvertisingResult,
                     weak_ptr_factory_.GetWeakPtr()));
  NotifyObservers();
}

void TargetDeviceBootstrapController::StopAdvertising() {
  DCHECK_EQ(status_.step, Step::ADVERTISING);

  // No pending requests.
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());

  connection_broker_->StopAdvertising(
      base::BindOnce(&TargetDeviceBootstrapController::OnStopAdvertising,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TargetDeviceBootstrapController::PrepareForUpdate() {
  if (status_.step != Step::CONNECTED || !authenticated_connection_) {
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
  constexpr Step kPossibleSteps[] = {Step::ADVERTISING,
                                     Step::QR_CODE_VERIFICATION};
  CHECK(base::Contains(kPossibleSteps, status_.step));

  pin_ = pin;
  // TODO: display pin
  status_.step = Step::PIN_VERIFICATION;
  status_.payload.emplace<absl::monostate>();
  NotifyObservers();
}

void TargetDeviceBootstrapController::OnQRCodeVerificationRequested(
    const std::vector<uint8_t>& qr_code_data) {
  constexpr Step kPossibleSteps[] = {Step::ADVERTISING};
  CHECK(base::Contains(kPossibleSteps, status_.step));

  auto qr_code = GenerateQRCode(qr_code_data);
  status_.step = Step::QR_CODE_VERIFICATION;
  status_.payload.emplace<QRCodePixelData>(std::move(qr_code));
  NotifyObservers();
}

void TargetDeviceBootstrapController::OnConnectionAuthenticated(
    base::WeakPtr<TargetDeviceConnectionBroker::AuthenticatedConnection>
        authenticated_connection) {
  constexpr Step kPossibleSteps[] = {Step::QR_CODE_VERIFICATION,
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
  status_.step = Step::ERROR;
  status_.payload = ErrorCode::CONNECTION_CLOSED;
  authenticated_connection_.reset();
  NotifyObservers();
}

void TargetDeviceBootstrapController::NotifyObservers() {
  for (auto& obs : observers_) {
    obs.OnStatusChanged(status_);
  }
}

void TargetDeviceBootstrapController::OnStartAdvertisingResult(bool success) {
  DCHECK_EQ(status_.step, Step::ADVERTISING);
  if (success) {
    return;
  }
  status_.step = Step::ERROR;
  status_.payload = ErrorCode::START_ADVERTISING_FAILED;
  NotifyObservers();
}

void TargetDeviceBootstrapController::OnStopAdvertising() {
  DCHECK_EQ(status_.step, Step::ADVERTISING);

  status_.step = Step::NONE;
  status_.payload.emplace<absl::monostate>();
  NotifyObservers();
}

void TargetDeviceBootstrapController::OnNotifySourceOfUpdateResponse(
    bool ack_successful) {
  CHECK(authenticated_connection_);

  if (ack_successful) {
    QS_LOG(INFO) << "Update ack sucessfully received. Preparing to resume "
                    "Quick Start after the update.";
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetBoolean(prefs::kShouldResumeQuickStartAfterReboot, true);
    base::Value::Dict info = connection_broker_->GetPrepareForUpdateInfo();
    prefs->SetDict(prefs::kResumeQuickStartAfterRebootInfo, std::move(info));
  }

  authenticated_connection_->Close(
      TargetDeviceConnectionBroker::ConnectionClosedReason::
          kTargetDeviceUpdate);
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
}

void TargetDeviceBootstrapController::AttemptGoogleAccountTransfer() {
  CHECK(authenticated_connection_);

  status_.step = Step::TRANSFERRING_GOOGLE_ACCOUNT_DETAILS;
  status_.payload.emplace<absl::monostate>();
  NotifyObservers();

  // TODO: Actually pass through a real challenge here.
  authenticated_connection_->RequestAccountTransferAssertion(
      "",
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
  status_.payload.emplace<absl::monostate>();
  NotifyObservers();
}

}  // namespace ash::quick_start
