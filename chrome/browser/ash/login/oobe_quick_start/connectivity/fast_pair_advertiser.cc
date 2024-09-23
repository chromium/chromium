// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fast_pair_advertiser.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/advertising_id.h"
#include "chromeos/constants/devicetype.h"

namespace ash::quick_start {

namespace {

constexpr const char kFastPairServiceUuid[] =
    "0000fe2c-0000-1000-8000-00805f9b34fb";
constexpr uint8_t kFastPairModelIdChromebook[] = {0x30, 0x68, 0x46};
constexpr uint8_t kFastPairModelIdChromebase[] = {0xe9, 0x31, 0x6c};
constexpr uint8_t kFastPairModelIdChromebox[] = {0xda, 0xde, 0x43};
constexpr uint16_t kCompanyId = 0x00e0;

QuickStartMetrics::FastPairAdvertisingErrorCode
MapBluetoothAdvertisementErrorCode(
    device::BluetoothAdvertisement::ErrorCode error_code) {
  switch (error_code) {
    case device::BluetoothAdvertisement::ErrorCode::ERROR_UNSUPPORTED_PLATFORM:
      return QuickStartMetrics::FastPairAdvertisingErrorCode::
          kUnsupportedPlatform;
    case device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_ALREADY_EXISTS:
      return QuickStartMetrics::FastPairAdvertisingErrorCode::
          kAdvertisementAlreadyExists;
    case device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_DOES_NOT_EXIST:
      return QuickStartMetrics::FastPairAdvertisingErrorCode::
          kAdvertisementDoesNotExist;
    case device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_INVALID_LENGTH:
      return QuickStartMetrics::FastPairAdvertisingErrorCode::
          kAdvertisementInvalidLength;
    case device::BluetoothAdvertisement::ErrorCode::
        ERROR_STARTING_ADVERTISEMENT:
      return QuickStartMetrics::FastPairAdvertisingErrorCode::
          kStartingAdvertisement;
    case device::BluetoothAdvertisement::ErrorCode::ERROR_RESET_ADVERTISING:
      return QuickStartMetrics::FastPairAdvertisingErrorCode::kResetAdvertising;
    case device::BluetoothAdvertisement::ErrorCode::ERROR_ADAPTER_POWERED_OFF:
      return QuickStartMetrics::FastPairAdvertisingErrorCode::
          kAdapterPoweredOff;
    case device::BluetoothAdvertisement::ErrorCode::
        ERROR_INVALID_ADVERTISEMENT_INTERVAL:
      return QuickStartMetrics::FastPairAdvertisingErrorCode::
          kInvalidAdvertisementInterval;
    case device::BluetoothAdvertisement::ErrorCode::
        INVALID_ADVERTISEMENT_ERROR_CODE:
      [[fallthrough]];
    default:
      return QuickStartMetrics::FastPairAdvertisingErrorCode::
          kInvalidAdvertisementErrorCode;
  }
}

std::vector<uint8_t> GetFastPairModelId() {
  switch (chromeos::GetDeviceType()) {
    case chromeos::DeviceType::kChromebook:
      return std::vector<uint8_t>(std::begin(kFastPairModelIdChromebook),
                                  std::end(kFastPairModelIdChromebook));
    case chromeos::DeviceType::kChromebase:
      return std::vector<uint8_t>(std::begin(kFastPairModelIdChromebase),
                                  std::end(kFastPairModelIdChromebase));
    case chromeos::DeviceType::kChromebox:
      return std::vector<uint8_t>(std::begin(kFastPairModelIdChromebox),
                                  std::end(kFastPairModelIdChromebox));
    default:
      return std::vector<uint8_t>(std::begin(kFastPairModelIdChromebook),
                                  std::end(kFastPairModelIdChromebook));
  }
}

}  // namespace

// static
FastPairAdvertiser::Factory* FastPairAdvertiser::Factory::factory_instance_ =
    nullptr;

// static
std::unique_ptr<FastPairAdvertiser> FastPairAdvertiser::Factory::Create(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (factory_instance_) {
    return factory_instance_->CreateInstance(adapter);
  }

  return std::make_unique<FastPairAdvertiser>(adapter);
}

// static
void FastPairAdvertiser::Factory::SetFactoryForTesting(
    FastPairAdvertiser::Factory* factory) {
  factory_instance_ = factory;
}

FastPairAdvertiser::FastPairAdvertiser(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK(adapter && adapter->IsPresent() && adapter->IsPowered());
  adapter_ = adapter;
  quick_start_metrics_ = std::make_unique<QuickStartMetrics>();
}

FastPairAdvertiser::~FastPairAdvertiser() {
  StopAdvertising(base::DoNothing());
}

void FastPairAdvertiser::AdvertisementReleased(
    device::BluetoothAdvertisement* advertisement) {
  StopAdvertising(base::DoNothing());
}

void FastPairAdvertiser::StartAdvertising(
    base::OnceCallback<void()> callback,
    base::OnceCallback<void()> error_callback,
    const AdvertisingId& advertising_id) {
  DCHECK(adapter_->IsPresent() && adapter_->IsPowered());
  DCHECK(!advertisement_);
  RegisterAdvertisement(std::move(callback), std::move(error_callback),
                        advertising_id);
}

void FastPairAdvertiser::StopAdvertising(base::OnceCallback<void()> callback) {
  if (!advertisement_) {
    std::move(callback).Run();
    // |this| might be destroyed here, do not access local fields.
    return;
  }

  UnregisterAdvertisement(std::move(callback));
}

void FastPairAdvertiser::RegisterAdvertisement(
    base::OnceClosure callback,
    base::OnceClosure error_callback,
    const AdvertisingId& advertising_id) {
  auto advertisement_data =
      std::make_unique<device::BluetoothAdvertisement::Data>(
          device::BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);

  device::BluetoothAdvertisement::UUIDList list;
  list.push_back(kFastPairServiceUuid);
  advertisement_data->set_service_uuids(std::move(list));

  device::BluetoothAdvertisement::ServiceData service_data;
  service_data.insert(
      std::make_pair(kFastPairServiceUuid, GetFastPairModelId()));
  advertisement_data->set_service_data(std::move(service_data));

  device::BluetoothAdvertisement::ManufacturerData manufacturer_data;
  std::vector<uint8_t> manufacturer_metadata =
      GenerateManufacturerMetadata(advertising_id);
  manufacturer_data.insert(
      std::make_pair(kCompanyId, std::move(manufacturer_metadata)));
  advertisement_data->set_manufacturer_data(std::move(manufacturer_data));

  adapter_->RegisterAdvertisement(
      std::move(advertisement_data),
      base::BindOnce(&FastPairAdvertiser::OnRegisterAdvertisement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      base::BindOnce(&FastPairAdvertiser::OnRegisterAdvertisementError,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(error_callback)));
}

void FastPairAdvertiser::OnRegisterAdvertisement(
    base::OnceClosure callback,
    scoped_refptr<device::BluetoothAdvertisement> advertisement) {
  quick_start_metrics_->RecordFastPairAdvertisementStarted(
      /*succeeded=*/true,
      /*error_code=*/std::nullopt);
  advertisement_ = advertisement;
  advertisement_->AddObserver(this);
  std::move(callback).Run();
}

void FastPairAdvertiser::OnRegisterAdvertisementError(
    base::OnceClosure error_callback,
    device::BluetoothAdvertisement::ErrorCode error_code) {
  LOG(ERROR) << __func__ << " failed with error code = " << error_code;
  quick_start_metrics_->RecordFastPairAdvertisementStarted(
      /*succeeded=*/false,
      /*error_code=*/MapBluetoothAdvertisementErrorCode(error_code));
  std::move(error_callback).Run();
  // |this| might be destroyed here, do not access local fields.
}

void FastPairAdvertiser::UnregisterAdvertisement(base::OnceClosure callback) {
  stop_callback_ = std::move(callback);
  advertisement_->RemoveObserver(this);
  advertisement_->Unregister(
      base::BindOnce(&FastPairAdvertiser::OnUnregisterAdvertisement,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FastPairAdvertiser::OnUnregisterAdvertisementError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairAdvertiser::OnUnregisterAdvertisement() {
  advertisement_.reset();
  quick_start_metrics_->RecordFastPairAdvertisementEnded(
      /*succeeded=*/true,
      /*error_code=*/std::nullopt);

  std::move(stop_callback_).Run();
  // |this| might be destroyed here, do not access local fields.
}

void FastPairAdvertiser::OnUnregisterAdvertisementError(
    device::BluetoothAdvertisement::ErrorCode error_code) {
  LOG(WARNING) << __func__ << " failed with error code = " << error_code;
  advertisement_.reset();
  quick_start_metrics_->RecordFastPairAdvertisementEnded(
      /*succeeded=*/false,
      /*error_code=*/MapBluetoothAdvertisementErrorCode(error_code));
  std::move(stop_callback_).Run();
  // |this| might be destroyed here, do not access local fields.
}

std::vector<uint8_t> FastPairAdvertiser::GenerateManufacturerMetadata(
    const AdvertisingId& advertising_id) {
  base::span<const uint8_t, AdvertisingId::kLength> id =
      advertising_id.AsBytes();
  std::vector<uint8_t> metadata(std::begin(id), std::end(id));
  return metadata;
}

}  // namespace ash::quick_start
