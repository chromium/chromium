// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_data_encryptor_impl.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_encryption.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "base/base64.h"
#include "base/memory/ptr_util.h"

namespace ash {
namespace quick_pair {

namespace {

// static
FastPairDataEncryptorImpl::Factory* g_test_factory_ = nullptr;

}  // namespace

// static
void FastPairDataEncryptorImpl::Factory::SetFactoryForTesting(
    Factory* g_test_factory) {
  g_test_factory_ = g_test_factory;
}

FastPairDataEncryptorImpl::Factory::~Factory() = default;

// static
void FastPairDataEncryptorImpl::Factory::CreateAsync(
    scoped_refptr<Device> device,
    base::OnceCallback<void(std::unique_ptr<FastPairDataEncryptor>)>
        on_get_instance_callback) {
  if (g_test_factory_) {
    g_test_factory_->CreateInstance(std::move(device),
                                    std::move(on_get_instance_callback));
    return;
  }

  FastPairRepository::Get()->GetDeviceMetadata(
      device->metadata_id,
      base::BindOnce(
          &FastPairDataEncryptorImpl::Factory::DeviceMetadataRetrieved,
          std::move(device), std::move(on_get_instance_callback)));
}

// static
void FastPairDataEncryptorImpl::Factory::DeviceMetadataRetrieved(
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
  fast_pair_encryption::KeyPair key_pair =
      fast_pair_encryption::GenerateKeysWithEcdhKeyAgreement(decoded_key);
  std::unique_ptr<FastPairDataEncryptorImpl> data_encryptor =
      base::WrapUnique(new FastPairDataEncryptorImpl(key_pair));
  std::move(on_get_instance_callback).Run(std::move(data_encryptor));
}

FastPairDataEncryptorImpl::FastPairDataEncryptorImpl(
    const fast_pair_encryption::KeyPair& key_pair)
    : secret_key_(key_pair.private_key), public_key_(key_pair.public_key) {}

FastPairDataEncryptorImpl::~FastPairDataEncryptorImpl() = default;

const std::array<uint8_t, kBlockSizeBytes>
FastPairDataEncryptorImpl::EncryptBytes(
    const std::array<uint8_t, kBlockSizeBytes>& bytes_to_encrypt) {
  return fast_pair_encryption::EncryptBytes(secret_key_, bytes_to_encrypt);
}

}  // namespace quick_pair
}  // namespace ash
