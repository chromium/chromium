// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/fast_initiation/fast_initiation_advertiser.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/nearby_sharing/fast_initiation/constants.h"
#include "components/cross_device/logging/logging.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace {
enum class FastInitVersion : uint8_t {
  kV1 = 0,
};

// This is the canonical service UUID used in outgoing Fast Initiation
// advertisements. The service UUID of 0xfe2c is shared with
// FastInitiationScanner in constants.h.
constexpr const char kFastInitiationServiceUuid[] =
    "0000fe2c-0000-1000-8000-00805f9b34fb";
const FastInitVersion kVersion = FastInitVersion::kV1;
const uint8_t kVersionBitmask = 0b111;
const uint8_t kTypeBitmask = 0b111;

// TODO(crbug.com/1099846): This value comes from Android, but we may need to
// find a more appropriate power setting for Chrome OS devices.
const int8_t kAdjustedTxPower = -66;

}  // namespace

// static
FastInitiationAdvertiser::Factory*
    FastInitiationAdvertiser::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<FastInitiationAdvertiser>
FastInitiationAdvertiser::Factory::Create(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (factory_instance_)
    return factory_instance_->CreateInstance(adapter);

  return std::make_unique<FastInitiationAdvertiser>(adapter);
}

// static
void FastInitiationAdvertiser::Factory::SetFactoryForTesting(
    FastInitiationAdvertiser::Factory* factory) {
  factory_instance_ = factory;
}

FastInitiationAdvertiser::FastInitiationAdvertiser(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK(adapter && adapter->IsPresent() && adapter->IsPowered());
  adapter_ = adapter;
}

FastInitiationAdvertiser::~FastInitiationAdvertiser() {
  StopAdvertising(base::DoNothing());
}

void FastInitiationAdvertiser::AdvertisementReleased(
    device::BluetoothAdvertisement* advertisement) {
  StopAdvertising(base::DoNothing());
}

void FastInitiationAdvertiser::StartAdvertising(
    FastInitType type,
    base::OnceCallback<void()> callback,
    base::OnceCallback<void()> error_callback) {
  DCHECK(adapter_->IsPresent() && adapter_->IsPowered());
  DCHECK(!advertisement_);
  RegisterAdvertisement(type, std::move(callback), std::move(error_callback));
}

void FastInitiationAdvertiser::StopAdvertising(
    base::OnceCallback<void()> callback) {
  if (!advertisement_) {
    std::move(callback).Run();
    // |this| might be destroyed here, do not access local fields.
    return;
  }

  UnregisterAdvertisement(std::move(callback));
}

void FastInitiationAdvertiser::RegisterAdvertisement(
    FastInitiationAdvertiser::FastInitType type,
    base::OnceClosure callback,
    base::OnceClosure error_callback) {
  auto advertisement_data =
      std::make_unique<device::BluetoothAdvertisement::Data>(
          device::BluetoothAdvertisement::ADVERTISEMENT_TYPE_BROADCAST);

  device::BluetoothAdvertisement::UUIDList list;
  list.push_back(kFastInitiationServiceUuid);
  advertisement_data->set_service_uuids(std::move(list));

  device::BluetoothAdvertisement::ServiceData service_data;
  auto payload = std::vector<uint8_t>(std::begin(kFastInitiationModelId),
                                      std::end(kFastInitiationModelId));
  auto metadata = GenerateFastInitV1Metadata(type);
  payload.insert(std::end(payload), std::begin(metadata), std::end(metadata));
  service_data.insert(std::pair<std::string, std::vector<uint8_t>>(
      kFastInitiationServiceUuid, payload));
  advertisement_data->set_service_data(std::move(service_data));

  adapter_->RegisterAdvertisement(
      std::move(advertisement_data),
      base::BindOnce(&FastInitiationAdvertiser::OnRegisterAdvertisement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      base::BindOnce(&FastInitiationAdvertiser::OnRegisterAdvertisementError,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(error_callback)));
}

void FastInitiationAdvertiser::OnRegisterAdvertisement(
    base::OnceClosure callback,
    scoped_refptr<device::BluetoothAdvertisement> advertisement) {
  advertisement_ = advertisement;
  advertisement_->AddObserver(this);
  std::move(callback).Run();
}

void FastInitiationAdvertiser::OnRegisterAdvertisementError(
    base::OnceClosure error_callback,
    device::BluetoothAdvertisement::ErrorCode error_code) {
  CD_LOG(ERROR, Feature::NS)
      << "FastInitiationAdvertiser::StartAdvertising() failed with "
         "error code = "
      << error_code;
  std::move(error_callback).Run();
  // |this| might be destroyed here, do not access local fields.
}

void FastInitiationAdvertiser::UnregisterAdvertisement(
    base::OnceClosure callback) {
  stop_callback_ = std::move(callback);
  advertisement_->RemoveObserver(this);
  advertisement_->Unregister(
      base::BindOnce(&FastInitiationAdvertiser::OnUnregisterAdvertisement,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&FastInitiationAdvertiser::OnUnregisterAdvertisementError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastInitiationAdvertiser::OnUnregisterAdvertisement() {
  advertisement_.reset();
  std::move(stop_callback_).Run();
  // |this| might be destroyed here, do not access local fields.
}

void FastInitiationAdvertiser::OnUnregisterAdvertisementError(
    device::BluetoothAdvertisement::ErrorCode error_code) {
  CD_LOG(WARNING, Feature::NS)
      << "FastInitiationAdvertiser::StopAdvertising() failed with error code = "
      << error_code;
  advertisement_.reset();
  std::move(stop_callback_).Run();
  // |this| might be destroyed here, do not access local fields.
}

std::vector<uint8_t> FastInitiationAdvertiser::GenerateFastInitV1Metadata(
    FastInitiationAdvertiser::FastInitType type) {
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
