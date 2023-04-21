// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker_factory.h"
#include "chrome/browser/ash/login/oobe_quick_start/oobe_quick_start_pref_names.h"
#include "chrome/browser/browser_process.h"
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
    base::WeakPtr<NearbyConnectionsManager> nearby_connections_manager)
    : connection_broker_(TargetDeviceConnectionBrokerFactory::Create(
          nearby_connections_manager,
          /*session_id=*/absl::nullopt)) {}

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
      this, /*use_pin_authentication=*/false,
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
  if (status_.step != Step::CONNECTED) {
    return;
  }

  // TODO(b/234655072): Trigger message to notify source device of update.
  // TODO(b/234655072): Implement timeout for connection to close.
  // If the source device successfully receives this message, it drops the
  // connection. The target device waits 1-3 seconds for the connection to close
  // in order to confirm the source device is prepared to re-connect after the
  // target device reboots. If the connection isn't closed within the timeout,
  // the target device reboots like normal and will not automatically resume
  // Quick Start after the update.
  prepare_for_update_on_connection_closed_ = true;
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
  constexpr Step kPossibleSteps[] = {Step::QR_CODE_VERIFICATION};
  CHECK(base::Contains(kPossibleSteps, status_.step));

  status_.step = Step::CONNECTED;
  status_.payload.emplace<absl::monostate>();
  NotifyObservers();
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
  NotifyObservers();

  if (prepare_for_update_on_connection_closed_) {
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetBoolean(prefs::kShouldResumeQuickStartAfterReboot, true);
    base::Value::Dict info = connection_broker_->GetPrepareForUpdateInfo();
    prefs->SetDict(prefs::kResumeQuickStartAfterRebootInfo, std::move(info));
  }
}

void TargetDeviceBootstrapController::NotifyObservers() {
  for (auto& obs : observers_) {
    obs.OnStatusChanged(status_);
  }
}

void TargetDeviceBootstrapController::OnStartAdvertisingResult(bool success) {
  DCHECK_EQ(status_.step, Step::ADVERTISING);
  if (success)
    return;
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

}  // namespace ash::quick_start
