// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_data_encryptor.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_encryption.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "base/base64.h"
#include "base/memory/ptr_util.h"

namespace ash {
namespace quick_pair {
namespace fast_pair_encryption {

// static
void FastPairDataEncryptor::Factory::CreateAsync(
    scoped_refptr<Device> device,
    base::OnceCallback<void(std::unique_ptr<FastPairDataEncryptor>)>
        on_get_instance_callback) {
  FastPairRepository::Get()->GetDeviceMetadata(
      device->metadata_id,
      base::BindOnce(&FastPairDataEncryptor::Factory::DeviceMetadataRetrieved,
                     std::move(device), std::move(on_get_instance_callback)));
}

// static
void FastPairDataEncryptor::Factory::DeviceMetadataRetrieved(
    scoped_refptr<Device> device,
    base::OnceCallback<void(std::unique_ptr<FastPairDataEncryptor>)>
        on_get_instance_callback,
    DeviceMetadata* device_metadata) {
  if (!device_metadata) {
    QP_LOG(WARNING) << "No device metadata retrieved.";
    std::move(on_get_instance_callback).Run(nullptr);
  }

  const std::string& public_anti_spoofing_key =
      device_metadata->device.anti_spoofing_key_pair().public_key();
  std::string decoded_key;
  base::Base64Decode(public_anti_spoofing_key, &decoded_key);
  KeyPair key_pair = GenerateKeysWithEcdhKeyAgreement(decoded_key);
  std::unique_ptr<FastPairDataEncryptor> data_encryptor =
      base::WrapUnique(new FastPairDataEncryptor(key_pair));
  std::move(on_get_instance_callback).Run(std::move(data_encryptor));
}

FastPairDataEncryptor::FastPairDataEncryptor(const KeyPair& key_pair)
    : secret_key_(key_pair.private_key), public_key_(key_pair.public_key) {}

FastPairDataEncryptor::~FastPairDataEncryptor() = default;

const std::array<uint8_t, kBlockSizeBytes> FastPairDataEncryptor::EncryptBytes(
    const std::array<uint8_t, kBlockSizeBytes>& bytes_to_encrypt) {
  return fast_pair_encryption::EncryptBytes(secret_key_, bytes_to_encrypt);
}

}  // namespace fast_pair_encryption
}  // namespace quick_pair
}  // namespace ash
