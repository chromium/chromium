// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/fast_initiation_manager.h"

#include <string>

#include "base/callback_helpers.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace {
enum class FastInitVersion : uint8_t {
  kV1 = 0,
};

constexpr const char kNearbySharingFastInitiationServiceUuid[] =
    "0000fe2c-0000-1000-8000-00805f9b34fb";
const uint8_t kNearbySharingFastPairId[] = {0xfc, 0x12, 0x8e};
const FastInitVersion kVersion = FastInitVersion::kV1;
const uint8_t kVersionBitmask = 0b111;
const uint8_t kTypeBitmask = 0b111;

// TODO(crbug.com/1099846): This value comes from Android, but we may need to
// find a more appropriate power setting for Chrome OS devices.
const int8_t kAdjustedTxPower = -66;

}  // namespace

// static
FastInitiationManager::Factory*
    FastInitiationManager::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<FastInitiationManager> FastInitiationManager::Factory::Create(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (factory_instance_)
    return factory_instance_->CreateInstance(adapter);

  return std::make_unique<FastInitiationManager>(adapter);
}

// static
void FastInitiationManager::Factory::SetFactoryForTesting(
    FastInitiationManager::Factory* factory) {
  factory_instance_ = factory;
}

FastInitiationManager::FastInitiationManager(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK(adapter && adapter->IsPresent() && adapter->IsPowered());
  adapter_ = adapter;
}

FastInitiationManager::~FastInitiationManager() {
  StopAdvertising(base::DoNothing());
}

void FastInitiationManager::AdvertisementReleased(
    device::BluetoothAdvertisement* advertisement) {
  StopAdvertising(base::DoNothing());
}

void FastInitiationManager::StartAdvertising(
    FastInitType type,
    base::OnceCallback<void()> callback,
    base::OnceCallback<void()> error_callback) {
  DCHECK(adapter_->IsPresent() && adapter_->IsPowered());
  DCHECK(!advertisement_);
  RegisterAdvertisement(type, std::move(callback), std::move(error_callback));
}

void FastInitiationManager::StopAdvertising(
    base::OnceCallback<void()> callback) {
  if (!advertisement_) {
    std::move(callback).Run();
    // |this| might be destroyed here, do not access local fields.
    return;
  }

  UnregisterAdvertisement(std::move(callback));
}

void FastInitiationManager::RegisterAdvertisement(
    FastInitiationManager::FastInitType type,
    base::OnceClosure callback,
    base::OnceClosure error_callback) {
  auto advertisement_data =
      std::make_unique<device::BluetoothAdvertisement::Data>(
          device::BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);

  auto list = std::make_unique<device::BluetoothAdvertisement::UUIDList>();
  list->push_back(kNearbySharingFastInitiationServiceUuid);
  advertisement_data->set_service_uuids(std::move(list));

  auto service_data =
      std::make_unique<device::BluetoothAdvertisement::ServiceData>();
  auto payload = std::vector<uint8_t>(std::begin(kNearbySharingFastPairId),
                                      std::end(kNearbySharingFastPairId));
  auto metadata = GenerateFastInitV1Metadata(type);
  payload.insert(std::end(payload), std::begin(metadata), std::end(metadata));
  service_data->insert(std::pair<std::string, std::vector<uint8_t>>(
      kNearbySharingFastInitiationServiceUuid, payload));
  advertisement_data->set_service_data(std::move(service_data));

  adapter_->RegisterAdvertisement(
      std::move(advertisement_data),
      base::BindOnce(&FastInitiationManager::OnRegisterAdvertisement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      base::BindOnce(&FastInitiationManager::OnRegisterAdvertisementError,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(error_callback)));
}

void FastInitiationManager::OnRegisterAdvertisement(
    base::OnceClosure callback,
    scoped_refptr<device::BluetoothAdvertisement> advertisement) {
  advertisement_ = advertisement;
  advertisement_->AddObserver(this);
  std::move(callback).Run();
}

void FastInitiationManager::OnRegisterAdvertisementError(
    base::OnceClosure error_callback,
    device::BluetoothAdvertisement::ErrorCode error_code) {
  NS_LOG(ERROR)
      << "FastInitiationManager::StartAdvertising() failed with error code = "
      << error_code;
  std::move(error_callback).Run();
  // |this| might be destroyed here, do not access local fields.
}

void FastInitiationManager::UnregisterAdvertisement(
    base::OnceClosure callback) {
  stop_callback_ = std::move(callback);
  advertisement_->RemoveObserver(this);
  advertisement_->Unregister(
      base::BindOnce(&FastInitiationManager::OnUnregisterAdvertisement,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FastInitiationManager::OnUnregisterAdvertisementError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastInitiationManager::OnUnregisterAdvertisement() {
  advertisement_.reset();
  std::move(stop_callback_).Run();
  // |this| might be destroyed here, do not access local fields.
}

void FastInitiationManager::OnUnregisterAdvertisementError(
    device::BluetoothAdvertisement::ErrorCode error_code) {
  NS_LOG(WARNING)
      << "FastInitiationManager::StopAdvertising() failed with error code = "
      << error_code;
  advertisement_.reset();
  std::move(stop_callback_).Run();
  // |this| might be destroyed here, do not access local fields.
}

std::vector<uint8_t> FastInitiationManager::GenerateFastInitV1Metadata(
    FastInitiationManager::FastInitType type) {
  std::vector<uint8_t> metadata;
  uint8_t versionConverted = (static_cast<uint8_t>(kVersion) & kVersionBitmask)
                             << 5;
  uint8_t typeConverted = (static_cast<uint8_t>(type) & kTypeBitmask) << 2;

  // Note: We convert this to a positive value before transport to align with
  // Android's behavior.
  int8_t powerConverted = -kAdjustedTxPower;

  // Note: the last two bits of this first byte correspond to 'uwb_enable' and
  // 'reserved'. The Chrome implementation does not support UWB (Ultra wideband)
  // and the 'reserved' bit is currently unused, so both are left empty.
  metadata.push_back(versionConverted | typeConverted);
  metadata.push_back(powerConverted);
  return metadata;
}
