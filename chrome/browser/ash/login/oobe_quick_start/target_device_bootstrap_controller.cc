// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/authenticated_connection.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/incoming_connection.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker_factory.h"
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
      this,
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

void TargetDeviceBootstrapController::OnIncomingConnectionInitiated(
    const std::string& source_device_id,
    base::WeakPtr<IncomingConnection> connection) {
  constexpr Step kPossibleSteps[] = {Step::ADVERTISING,
                                     Step::QR_CODE_VERIFICATION};
  DCHECK(base::Contains(kPossibleSteps, status_.step));
  if (status_.step == Step::QR_CODE_VERIFICATION) {
    // New connection came. It should be a different device.
    DCHECK_NE(source_device_id_, source_device_id);
  }
  source_device_id_ = source_device_id;
  incoming_connection_ = std::move(connection);
  auto qr_code = GenerateQRCode(incoming_connection_->GetQrCodeData());
  status_.step = Step::QR_CODE_VERIFICATION;
  status_.payload.emplace<QRCodePixelData>(std::move(qr_code));
  NotifyObservers();
}

void TargetDeviceBootstrapController::OnConnectionAuthenticated(
    const std::string& source_device_id,
    base::WeakPtr<AuthenticatedConnection> connection) {
  DCHECK_EQ(source_device_id_, source_device_id);
  constexpr Step kPossibleSteps[] = {Step::QR_CODE_VERIFICATION};
  DCHECK(base::Contains(kPossibleSteps, status_.step));
  DCHECK(incoming_connection_.WasInvalidated());

  status_.step = Step::CONNECTED;
  status_.payload.emplace<absl::monostate>();
  NotifyObservers();
}

void TargetDeviceBootstrapController::OnConnectionRejected(
    const std::string& source_device_id) {
  DCHECK_EQ(source_device_id_, source_device_id);
  status_.step = Step::ERROR;
  status_.payload = ErrorCode::CONNECTION_REJECTED;
  NotifyObservers();
}

void TargetDeviceBootstrapController::OnConnectionClosed(
    const std::string& source_device_id) {
  DCHECK_EQ(source_device_id_, source_device_id);
  status_.step = Step::ERROR;
  status_.payload = ErrorCode::CONNECTION_CLOSED;
  NotifyObservers();
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
