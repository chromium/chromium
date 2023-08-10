// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker_impl.h"

#include <memory>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fast_pair_advertiser.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/nearby/quick_start_connectivity_service.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connections_manager.h"
#include "chromeos/ash/components/quick_start/logging.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash::quick_start {

namespace {

// Endpoint Info version number, currently version 1.
constexpr uint8_t kEndpointInfoVersion = 1;

// Smart Setup verification style, e.g. QR code, pin, etc.
// 5 = "OUT_OF_BAND", which tells the phone to scan for the QR code.
// 6 = "DIGITS", which tells the phone to display a PIN for the user to match.//
// Values come from the TargetConnectionInfo VerificationStyle enum:
// http://google3/logs/proto/wireless/android/smartsetup/smart_setup_extension.proto;l=894;rcl=489739066
constexpr uint8_t kEndpointInfoVerificationStyleOutOfBand = 5;
constexpr uint8_t kEndpointInfoVerificationStyleDigits = 6;
// Device Type for Smart Setup, e.g. phone, tablet.  8 = "Chrome"
// Values come from the DiscoveryEvent DeviceType enum:
// http://google3/logs/proto/wireless/android/smartsetup/smart_setup_extension.proto;l=985;rcl=507029311
constexpr uint8_t kEndpointInfoDeviceType = 8;

// Boolean field indicating to Smart Setup whether the client is Quick Start.
constexpr uint8_t kEndpointInfoIsQuickStart = 1;

// Boolean field indicating whether the target device supports LSKF
// verification.
constexpr uint8_t kEndpointInfoPreferTargetUserVerification = 0;

constexpr size_t kMaxEndpointInfoDisplayNameLength = 18;

// The Advertising Id field has a fixed width of 10 bytes, but contains a
// base64-encoded UTF-8 string that will be null-terminated if less than 10
// characters.
constexpr size_t kEndpointInfoAdvertisingIdLength = 10;

// Base64 padding character
constexpr char kBase64PaddingChar = '=';

constexpr base::TimeDelta kNearbyConnectionsAdvertisementAfterUpdateTimeout =
    base::Seconds(30);

// The display name must:
// - Be a variable-length string of utf-8 bytes
// - Be at most 18 bytes
// - If less than 18 bytes, must be null-terminated
std::vector<uint8_t> GetEndpointInfoDisplayNameBytes(
    const RandomSessionId& session_id) {
  std::string display_name = base::UTF16ToUTF8(ui::GetChromeOSDeviceName());
  std::string suffix = " (" + session_id.GetDisplayCode() + ")";

  base::TruncateUTF8ToByteSize(
      display_name, kMaxEndpointInfoDisplayNameLength - suffix.size(),
      &display_name);
  display_name += suffix;

  std::vector<uint8_t> display_name_bytes(display_name.begin(),
                                          display_name.end());
  if (display_name_bytes.size() < kMaxEndpointInfoDisplayNameLength) {
    display_name_bytes.push_back(0);
  }

  return display_name_bytes;
}

std::vector<uint8_t> Base64EncodeOmitPadding(
    const std::vector<uint8_t>& bytes) {
  std::string input(bytes.begin(), bytes.end());
  std::string output;
  base::Base64Encode(input, &output);

  // Strip padding characters from end.
  const size_t last_non_padding_pos =
      output.find_last_not_of(kBase64PaddingChar);
  if (last_non_padding_pos != std::string::npos) {
    output.resize(last_non_padding_pos + 1);
  }

  return std::vector<uint8_t>(output.begin(), output.end());
}

}  // namespace

void TargetDeviceConnectionBrokerImpl::BluetoothAdapterFactoryWrapper::
    GetAdapter(device::BluetoothAdapterFactory::AdapterCallback callback) {
  if (bluetooth_adapter_factory_wrapper_for_testing_) {
    bluetooth_adapter_factory_wrapper_for_testing_->GetAdapterImpl(
        std::move(callback));
    return;
  }

  device::BluetoothAdapterFactory* adapter_factory =
      device::BluetoothAdapterFactory::Get();

  // Bluetooth is always supported on the ChromeOS platform.
  DCHECK(adapter_factory->IsBluetoothSupported());

  adapter_factory->GetAdapter(std::move(callback));
}

TargetDeviceConnectionBrokerImpl::BluetoothAdapterFactoryWrapper*
    TargetDeviceConnectionBrokerImpl::BluetoothAdapterFactoryWrapper::
        bluetooth_adapter_factory_wrapper_for_testing_ = nullptr;

TargetDeviceConnectionBrokerImpl::TargetDeviceConnectionBrokerImpl(
    SessionContext session_context,
    QuickStartConnectivityService* quick_start_connectivity_service,
    std::unique_ptr<Connection::Factory> connection_factory)
    : session_context_(session_context),
      quick_start_connectivity_service_(quick_start_connectivity_service),
      connection_factory_(std::move(connection_factory)) {
  is_resume_after_update_ = session_context_.is_resume_after_update();
  GetBluetoothAdapter();
}

TargetDeviceConnectionBrokerImpl::~TargetDeviceConnectionBrokerImpl() {}

TargetDeviceConnectionBrokerImpl::FeatureSupportStatus
TargetDeviceConnectionBrokerImpl::GetFeatureSupportStatus() const {
  if (!bluetooth_adapter_) {
    return FeatureSupportStatus::kUndetermined;
  }

  if (bluetooth_adapter_->IsPresent()) {
    return FeatureSupportStatus::kSupported;
  }

  return FeatureSupportStatus::kNotSupported;
}

void TargetDeviceConnectionBrokerImpl::GetBluetoothAdapter() {
  // Because this will be called from the constructor, GetAdapter() may call
  // OnGetBluetoothAdapter() immediately which can cause problems during tests
  // since the class is not fully constructed yet.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BluetoothAdapterFactoryWrapper::GetAdapter,
          base::BindOnce(
              &TargetDeviceConnectionBrokerImpl::OnGetBluetoothAdapter,
              weak_ptr_factory_.GetWeakPtr())));
}

void TargetDeviceConnectionBrokerImpl::OnGetBluetoothAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
  MaybeNotifyFeatureStatus();

  if (deferred_start_advertising_callback_) {
    std::move(deferred_start_advertising_callback_).Run();
  }
}

void TargetDeviceConnectionBrokerImpl::StartAdvertising(
    ConnectionLifecycleListener* listener,
    bool use_pin_authentication,
    ResultCallback on_start_advertising_callback) {
  if (GetFeatureSupportStatus() == FeatureSupportStatus::kUndetermined) {
    deferred_start_advertising_callback_ = base::BindOnce(
        &TargetDeviceConnectionBroker::StartAdvertising,
        weak_ptr_factory_.GetWeakPtr(), listener, use_pin_authentication,
        std::move(on_start_advertising_callback));
    return;
  }

  if (GetFeatureSupportStatus() == FeatureSupportStatus::kNotSupported) {
    LOG(ERROR)
        << __func__
        << " failed to start advertising because the feature is not supported.";
    std::move(on_start_advertising_callback).Run(/*success=*/false);
    return;
  }

  DCHECK(GetFeatureSupportStatus() == FeatureSupportStatus::kSupported);

  if (!bluetooth_adapter_->IsPowered()) {
    LOG(ERROR) << __func__
               << " failed to start advertising because the bluetooth adapter "
                  "is not powered.";
    std::move(on_start_advertising_callback).Run(/*success=*/false);
    return;
  }

  use_pin_authentication_ = use_pin_authentication;
  connection_lifecycle_listener_ = listener;

  if (is_resume_after_update_) {
    StartNearbyConnectionsAdvertising(std::move(on_start_advertising_callback));
  } else {
    // This will start Nearby Connections advertising if Fast Pair advertising
    // succeeds.
    StartFastPairAdvertising(std::move(on_start_advertising_callback));
  }
}

void TargetDeviceConnectionBrokerImpl::StartFastPairAdvertising(
    ResultCallback callback) {
  QS_LOG(INFO) << "Starting Fast Pair advertising with session id "
               << session_context_.random_session_id() << " ("
               << session_context_.random_session_id().GetDisplayCode() << ")";

  fast_pair_advertiser_ =
      FastPairAdvertiser::Factory::Create(bluetooth_adapter_);
  auto [success_callback, failure_callback] =
      base::SplitOnceCallback(std::move(callback));

  fast_pair_advertiser_->StartAdvertising(
      base::BindOnce(
          &TargetDeviceConnectionBrokerImpl::OnStartFastPairAdvertisingSuccess,
          weak_ptr_factory_.GetWeakPtr(), std::move(success_callback)),
      base::BindOnce(
          &TargetDeviceConnectionBrokerImpl::OnStartFastPairAdvertisingError,
          weak_ptr_factory_.GetWeakPtr(), std::move(failure_callback)),
      session_context_.random_session_id(), use_pin_authentication_);
}

void TargetDeviceConnectionBrokerImpl::OnStartFastPairAdvertisingSuccess(
    ResultCallback callback) {
  QS_LOG(INFO) << "Fast Pair advertising started successfully.";
  StartNearbyConnectionsAdvertising(std::move(callback));
}

void TargetDeviceConnectionBrokerImpl::OnStartFastPairAdvertisingError(
    ResultCallback callback) {
  QS_LOG(ERROR) << "Fast Pair advertising failed to start.";
  fast_pair_advertiser_.reset();
  std::move(callback).Run(/*success=*/false);
}

void TargetDeviceConnectionBrokerImpl::StopAdvertising(
    base::OnceClosure on_stop_advertising_callback) {
  if (deferred_start_advertising_callback_) {
    deferred_start_advertising_callback_.Reset();
  }

  if (!fast_pair_advertiser_) {
    QS_LOG(INFO) << __func__ << " Not currently advertising, ignoring.";
    std::move(on_stop_advertising_callback).Run();
    return;
  }

  fast_pair_advertiser_->StopAdvertising(base::BindOnce(
      &TargetDeviceConnectionBrokerImpl::OnStopFastPairAdvertising,
      weak_ptr_factory_.GetWeakPtr(), std::move(on_stop_advertising_callback)));
}

std::string TargetDeviceConnectionBrokerImpl::GetSessionIdDisplayCode() {
  return session_context_.random_session_id().GetDisplayCode();
}

void TargetDeviceConnectionBrokerImpl::OnStopFastPairAdvertising(
    base::OnceClosure callback) {
  fast_pair_advertiser_.reset();

  StopNearbyConnectionsAdvertising(std::move(callback));
}

// The EndpointInfo consists of the following fields:
// - EndpointInfo version number, 1 byte
// - Display name, max 18 bytes (see GetEndpointInfoDisplayNameBytes())
// - Advertisement data, 60 bytes, base64 encoded:
//   - Verification Style, byte[0]
//   - Device Type, byte[1]
//   - Advertising Id, byte[2-11], 10 UTF-8 bytes. (See RandomSessionId)
//   - isQuickStart, byte[12], =1 for Quick Start.
//   - preferTargetUserVerification, byte[13], =0 for ChromeOS.
//   - Pad with zeros to 60 bytes. Extra space reserved for futureproofing.
std::vector<uint8_t> TargetDeviceConnectionBrokerImpl::GenerateEndpointInfo()
    const {
  std::string session_id = session_context_.random_session_id().ToString();
  std::vector<uint8_t> display_name_bytes =
      GetEndpointInfoDisplayNameBytes(session_context_.random_session_id());
  uint8_t verification_style = use_pin_authentication_
                                   ? kEndpointInfoVerificationStyleDigits
                                   : kEndpointInfoVerificationStyleOutOfBand;

  std::vector<uint8_t> advertisement_data;
  advertisement_data.reserve(60);
  advertisement_data.push_back(verification_style);
  advertisement_data.push_back(kEndpointInfoDeviceType);
  advertisement_data.insert(advertisement_data.end(), session_id.begin(),
                            session_id.end());
  for (size_t i = 0; i < kEndpointInfoAdvertisingIdLength - session_id.size();
       i++) {
    // Pad out the advertising id to the correct field length using null
    // terminators.
    advertisement_data.push_back(0);
  }
  advertisement_data.push_back(kEndpointInfoIsQuickStart);
  advertisement_data.push_back(kEndpointInfoPreferTargetUserVerification);
  while (advertisement_data.size() < 60) {
    // Reserve 60 bytes for the advertisement data for futureproofing.
    advertisement_data.push_back(0);
  }
  std::vector<uint8_t> advertisement_data_b64 =
      Base64EncodeOmitPadding(advertisement_data);

  std::vector<uint8_t> endpoint_info;
  endpoint_info.push_back(kEndpointInfoVersion);
  endpoint_info.insert(endpoint_info.end(), display_name_bytes.begin(),
                       display_name_bytes.end());
  endpoint_info.insert(endpoint_info.end(), advertisement_data_b64.begin(),
                       advertisement_data_b64.end());

  return endpoint_info;
}

void TargetDeviceConnectionBrokerImpl::StartNearbyConnectionsAdvertising(
    ResultCallback callback) {
  CHECK(quick_start_connectivity_service_);
  QS_LOG(INFO) << "Starting Nearby Connections Advertising";
  // TODO(b/234655072): PowerLevel::kHighPower implies using Bluetooth classic,
  // but we should also advertise over BLE. Nearby Connections does not yet
  // support BLE as an upgrade medium, so Quick Start over BLE is planned for
  // post-launch.
  quick_start_connectivity_service_->GetNearbyConnectionsManager()
      ->StartAdvertising(
          GenerateEndpointInfo(), /*listener=*/this, PowerLevel::kHighPower,
          DataUsage::kOffline,
          base::BindOnce(&TargetDeviceConnectionBrokerImpl::
                             OnStartNearbyConnectionsAdvertising,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TargetDeviceConnectionBrokerImpl::StopNearbyConnectionsAdvertising(
    base::OnceClosure callback) {
  QS_LOG(INFO) << "Stopping Nearby Connections Advertising";
  quick_start_connectivity_service_->GetNearbyConnectionsManager()
      ->StopAdvertising(base::BindOnce(
          &TargetDeviceConnectionBrokerImpl::OnStopNearbyConnectionsAdvertising,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TargetDeviceConnectionBrokerImpl::OnStartNearbyConnectionsAdvertising(
    ResultCallback callback,
    NearbyConnectionsManager::ConnectionsStatus status) {
  QS_LOG(INFO) << "Nearby Connections Advertising started with status "
               << status;
  bool success =
      status == NearbyConnectionsManager::ConnectionsStatus::kSuccess;

  if (success && is_resume_after_update_) {
    nearby_connections_advertisement_after_update_timeout_timer_.Start(
        FROM_HERE, kNearbyConnectionsAdvertisementAfterUpdateTimeout,
        base::BindOnce(&TargetDeviceConnectionBrokerImpl::
                           OnNearbyConnectionsAdvertisementAfterUpdateTimeout,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  std::move(callback).Run(success);
}

void TargetDeviceConnectionBrokerImpl::OnStopNearbyConnectionsAdvertising(
    base::OnceClosure callback,
    NearbyConnectionsManager::ConnectionsStatus status) {
  QS_LOG(INFO) << "Nearby Connections Advertising stopped with status "
               << status;
  if (status != NearbyConnectionsManager::ConnectionsStatus::kSuccess) {
    QS_LOG(WARNING) << "Failed to stop Nearby Connections advertising";
  }
  std::move(callback).Run();
}

void TargetDeviceConnectionBrokerImpl::OnIncomingConnectionInitiated(
    const std::string& endpoint_id,
    const std::vector<uint8_t>& endpoint_info) {
  if (is_resume_after_update_) {
    QS_LOG(INFO) << "Skipped manual verification and will attempt an "
                    "\"automatic handshake\": endpoint_id="
                 << endpoint_id;
    return;
  }

  QS_LOG(INFO) << "Incoming Nearby Connection Initiated: endpoint_id="
               << endpoint_id
               << " use_pin_authentication=" << use_pin_authentication_;

  CHECK(connection_lifecycle_listener_);
  if (use_pin_authentication_) {
    absl::optional<std::string> auth_token =
        quick_start_connectivity_service_->GetNearbyConnectionsManager()
            ->GetAuthenticationToken(endpoint_id);
    CHECK(auth_token);
    std::string pin = DerivePin(*auth_token);
    QS_LOG(INFO) << "Incoming Nearby Connection Initiated: pin=" << pin;
    connection_lifecycle_listener_->OnPinVerificationRequested(pin);
  }
}

void TargetDeviceConnectionBrokerImpl::OnIncomingConnectionAccepted(
    const std::string& endpoint_id,
    const std::vector<uint8_t>& endpoint_info,
    NearbyConnection* nearby_connection) {
  // Nearby Connections advertisement succeeded when connection is accepted so
  // stop timer when running.
  if (nearby_connections_advertisement_after_update_timeout_timer_
          .IsRunning()) {
    nearby_connections_advertisement_after_update_timeout_timer_.Stop();
  }

  QS_LOG(INFO) << "Incoming Nearby Connection Accepted: endpoint_id="
               << endpoint_id;

  // TODO(b/234655072): Handle Connection Closed in the Connection Broker
  connection_ = connection_factory_->Create(
      nearby_connection, session_context_,
      quick_start_connectivity_service_->GetQuickStartDecoder(),
      base::BindOnce(&TargetDeviceConnectionBrokerImpl::OnConnectionClosed,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &TargetDeviceConnectionBrokerImpl::OnConnectionAuthenticated,
          weak_ptr_factory_.GetWeakPtr()));

  if (use_pin_authentication_ && !is_resume_after_update_) {
    QS_LOG(INFO) << "Pin authentication completed!";
    quick_start_metrics::RecordHandshakeStarted(/*handshake_started=*/false);
    connection_->MarkConnectionAuthenticated();
  } else {
    QS_LOG(INFO) << "Initiating cryptographic handshake.";
    absl::optional<std::string> auth_token =
        quick_start_connectivity_service_->GetNearbyConnectionsManager()
            ->GetAuthenticationToken(endpoint_id);
    CHECK(auth_token);
    quick_start_metrics::RecordHandshakeStarted(/*handshake_started=*/true);
    connection_->InitiateHandshake(
        *auth_token,
        base::BindOnce(&TargetDeviceConnectionBrokerImpl::OnHandshakeCompleted,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void TargetDeviceConnectionBrokerImpl::OnHandshakeCompleted(bool success) {
  CHECK(connection_);
  if (!success) {
    QS_LOG(ERROR) << "Handshake failed! Dropping the connection.";
    connection_->Close(ConnectionClosedReason::kAuthenticationFailed);
    return;
  }

  QS_LOG(INFO) << "Handshake succeeded!";
  connection_->MarkConnectionAuthenticated();
}

void TargetDeviceConnectionBrokerImpl::
    OnNearbyConnectionsAdvertisementAfterUpdateTimeout() {
  is_resume_after_update_ = false;
  QS_LOG(ERROR) << "The Nearby Connections advertisement timed out during "
                   "attempt to automatically resume after an update. Will now "
                   "attempt to stop Nearby Connections advertising and "
                   "fallback to Fast Pair advertising.";
  auto start_fast_pair_advertising = base::BindOnce(
      &TargetDeviceConnectionBrokerImpl::StartFastPairAdvertising,
      weak_ptr_factory_.GetWeakPtr(), base::DoNothing());
  StopNearbyConnectionsAdvertising(std::move(start_fast_pair_advertising));
}
}  // namespace ash::quick_start
