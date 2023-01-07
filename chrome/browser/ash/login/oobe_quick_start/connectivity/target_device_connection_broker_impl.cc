// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker_impl.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fast_pair_advertiser.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash::quick_start {

namespace {

// Endpoint Info version number, currently version 1.
constexpr uint8_t kEndpointInfoVersion = 1;

// Smart Setup verification style, e.g. shapes, pin, etc.
// 0 = "Default", since there isn't yet a QR code option.
// Values come from this enum:
// http://google3/logs/proto/wireless/android/smartsetup/smart_setup_extension.proto;l=876;rcl=458110957
constexpr uint8_t kEndpointInfoVerificationStyle = 0;

// Device Type for Smart Setup, e.g. phone, tablet.
// 0 = "Unknown", since there isn't yet a Chromebook option.
// Values come from this enum:
// http://google3/logs/proto/wireless/android/smartsetup/smart_setup_extension.proto;l=961;rcl=458110957
constexpr uint8_t kEndpointInfoDeviceType = 0;

// Boolean field indicating to Smart Setup whether the client is Quick Start.
constexpr uint8_t kEndpointInfoIsQuickStart = 1;

constexpr size_t kMaxEndpointInfoDisplayNameLength = 18;

// Derive three decimal digits from the RandomSessionId.
std::string GetDisplayNameSessionIdDigits(const RandomSessionId& session_id) {
  base::span<const uint8_t, RandomSessionId::kLength> session_id_bytes =
      session_id.AsBytes();
  uint32_t high = session_id_bytes[0];
  uint32_t low = session_id_bytes[1];
  uint32_t x = (high << 8) + low;
  return base::NumberToString(x % 1000);
}

// The display name must:
// - Be a variable-length string of utf-8 bytes
// - Be at most 18 bytes
// - If less than 18 bytes, must be null-terminated
std::vector<uint8_t> GetEndpointInfoDisplayNameBytes(
    const RandomSessionId& session_id) {
  std::string display_name = base::UTF16ToUTF8(ui::GetChromeOSDeviceName());
  std::string suffix = " (" + GetDisplayNameSessionIdDigits(session_id) + ")";

  base::TruncateUTF8ToByteSize(
      display_name, kMaxEndpointInfoDisplayNameLength - suffix.size(),
      &display_name);
  display_name += suffix;

  std::vector<uint8_t> display_name_bytes(display_name.begin(),
                                          display_name.end());
  if (display_name_bytes.size() < kMaxEndpointInfoDisplayNameLength)
    display_name_bytes.push_back(0);

  return display_name_bytes;
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
    RandomSessionId session_id,
    base::WeakPtr<NearbyConnectionsManager> nearby_connections_manager)
    : random_session_id_(session_id),
      nearby_connections_manager_(nearby_connections_manager) {
  GetBluetoothAdapter();
}

TargetDeviceConnectionBrokerImpl::~TargetDeviceConnectionBrokerImpl() {}

TargetDeviceConnectionBrokerImpl::FeatureSupportStatus
TargetDeviceConnectionBrokerImpl::GetFeatureSupportStatus() const {
  if (!bluetooth_adapter_)
    return FeatureSupportStatus::kUndetermined;

  if (bluetooth_adapter_->IsPresent())
    return FeatureSupportStatus::kSupported;

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
    ResultCallback on_start_advertising_callback) {
  // TODO(b/234655072): Notify client about incoming connections on the started
  // advertisement via ConnectionLifecycleListener.
  if (GetFeatureSupportStatus() == FeatureSupportStatus::kUndetermined) {
    deferred_start_advertising_callback_ =
        base::BindOnce(&TargetDeviceConnectionBroker::StartAdvertising,
                       weak_ptr_factory_.GetWeakPtr(), listener,
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

  VLOG(1) << "Starting advertising with session id " << random_session_id_
          << " (" << GetDisplayNameSessionIdDigits(random_session_id_) << ")";

  fast_pair_advertiser_ =
      FastPairAdvertiser::Factory::Create(bluetooth_adapter_);
  auto [success_callback, failure_callback] =
      base::SplitOnceCallback(std::move(on_start_advertising_callback));

  fast_pair_advertiser_->StartAdvertising(
      // TODO(b/234655072): on success, start Nearby Connections advertising.
      base::BindOnce(std::move(success_callback), /*success=*/true),
      base::BindOnce(
          &TargetDeviceConnectionBrokerImpl::OnStartFastPairAdvertisingError,
          weak_ptr_factory_.GetWeakPtr(), std::move(failure_callback)),
      random_session_id_);
}

void TargetDeviceConnectionBrokerImpl::OnStartFastPairAdvertisingError(
    ResultCallback callback) {
  fast_pair_advertiser_.reset();
  std::move(callback).Run(/*success=*/false);
}

void TargetDeviceConnectionBrokerImpl::StopAdvertising(
    base::OnceClosure on_stop_advertising_callback) {
  if (deferred_start_advertising_callback_) {
    deferred_start_advertising_callback_.Reset();
  }

  if (!fast_pair_advertiser_) {
    VLOG(1) << __func__ << " Not currently advertising, ignoring.";
    std::move(on_stop_advertising_callback).Run();
    return;
  }

  fast_pair_advertiser_->StopAdvertising(base::BindOnce(
      &TargetDeviceConnectionBrokerImpl::OnStopFastPairAdvertising,
      weak_ptr_factory_.GetWeakPtr(), std::move(on_stop_advertising_callback)));
}

void TargetDeviceConnectionBrokerImpl::OnStopFastPairAdvertising(
    base::OnceClosure callback) {
  fast_pair_advertiser_.reset();
  std::move(callback).Run();
}

// The EndpointInfo consists of the following fields:
// - EndpointInfo version number, 1 byte
// - Display name, max 18 bytes (see GetEndpointInfoDisplayNameBytes())
// - Advertisement data, 13 bytes:
//   - Verification Style, byte[0]
//   - Device Type, byte[1]
//   - Advertising Id, byte[2-11], 10 bytes. (See RandomSessionId)
//   - isQuickStart, byte[12], =1 for Quick Start.
std::vector<uint8_t> TargetDeviceConnectionBrokerImpl::GenerateEndpointInfo() {
  base::span<const uint8_t, RandomSessionId::kLength> session_id_bytes =
      random_session_id_.AsBytes();
  std::vector<uint8_t> display_name_bytes =
      GetEndpointInfoDisplayNameBytes(random_session_id_);

  std::vector<uint8_t> endpoint_info;
  endpoint_info.reserve(32);

  endpoint_info.push_back(kEndpointInfoVersion);
  endpoint_info.insert(endpoint_info.end(), display_name_bytes.begin(),
                       display_name_bytes.end());
  endpoint_info.push_back(kEndpointInfoVerificationStyle);
  endpoint_info.push_back(kEndpointInfoDeviceType);
  endpoint_info.insert(endpoint_info.end(), session_id_bytes.begin(),
                       session_id_bytes.end());
  endpoint_info.push_back(kEndpointInfoIsQuickStart);

  return endpoint_info;
}

}  // namespace ash::quick_start
