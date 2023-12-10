// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor_impl.h"

#include <array>
#include <cstdint>

#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_encryption.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process.h"
#include "components/cross_device/logging/logging.h"

namespace ash {
namespace quick_pair {

namespace {

// static
FastPairDataEncryptorImpl::Factory* g_test_factory_ = nullptr;

bool ValidateInputSize(const std::vector<uint8_t>& encrypted_bytes) {
  if (encrypted_bytes.size() != kBlockSizeBytes) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Encrypted bytes should have size = " << kBlockSizeBytes
        << ", actual =  " << encrypted_bytes.size();
    return false;
  }

  return true;
}

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

  if (device->protocol() == Protocol::kFastPairInitial ||
      device->protocol() == Protocol::kFastPairRetroactive) {
    CreateAsyncWithKeyExchange(std::move(device),
                               std::move(on_get_instance_callback));
  } else if (device->protocol() == Protocol::kFastPairSubsequent) {
    CreateAsyncWithAccountKey(std::move(device),
                              std::move(on_get_instance_callback));
  } else {
    // This object doesn't handle any other protocols, calling with another
    // is a bug.
    NOTREACHED();
  }
}

// static
void FastPairDataEncryptorImpl::Factory::CreateAsyncWithKeyExchange(
    scoped_refptr<Device> device,
    base::OnceCallback<void(std::unique_ptr<FastPairDataEncryptor>)>
        on_get_instance_callback) {
  CD_LOG(INFO, Feature::FP) << __func__;

  // We first have to get the metadata in order to get the public key to use
  // to generate the new secret key pair.
  auto metadata_id = device->metadata_id();
  FastPairRepository::Get()->GetDeviceMetadata(
      metadata_id,
      base::BindOnce(
          &FastPairDataEncryptorImpl::Factory::DeviceMetadataRetrieved,
          std::move(device), std::move(on_get_instance_callback)));
}

// static
void FastPairDataEncryptorImpl::Factory::CreateAsyncWithAccountKey(
    scoped_refptr<Device> device,
    base::OnceCallback<void(std::unique_ptr<FastPairDataEncryptor>)>
        on_get_instance_callback) {
  CD_LOG(INFO, Feature::FP) << __func__;

  std::optional<std::vector<uint8_t>> account_key = device->account_key();
  DCHECK(account_key);
  DCHECK_EQ(account_key->size(), static_cast<size_t>(kPrivateKeyByteSize));

  std::array<uint8_t, kPrivateKeyByteSize> private_key;
  std::copy_n(account_key->begin(), kPrivateKeyByteSize, private_key.begin());

  std::unique_ptr<FastPairDataEncryptorImpl> data_encryptor =
      base::WrapUnique(new FastPairDataEncryptorImpl(std::move(private_key)));
  std::move(on_get_instance_callback).Run(std::move(data_encryptor));
}

// static
void FastPairDataEncryptorImpl::Factory::DeviceMetadataRetrieved(
    scoped_refptr<Device> device,
    base::OnceCallback<void(std::unique_ptr<FastPairDataEncryptor>)>
        on_get_instance_callback,
    DeviceMetadata* device_metadata,
    bool has_retryable_error) {
  if (!device_metadata) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No device metadata retrieved.";
    std::move(on_get_instance_callback).Run(nullptr);
    return;
  }

  const std::string& public_anti_spoofing_key =
      device_metadata->GetDetails().anti_spoofing_key_pair().public_key();
  std::optional<fast_pair_encryption::KeyPair> key_pair =
      fast_pair_encryption::GenerateKeysWithEcdhKeyAgreement(
          public_anti_spoofing_key);

  RecordKeyPairGenerationResult(/*success=*/key_pair.has_value());

  if (key_pair) {
    std::unique_ptr<FastPairDataEncryptorImpl> data_encryptor =
        base::WrapUnique(new FastPairDataEncryptorImpl(key_pair.value()));
    std::move(on_get_instance_callback).Run(std::move(data_encryptor));
  } else {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Failed to get key pair for device";
    std::move(on_get_instance_callback).Run(nullptr);
  }
}

FastPairDataEncryptorImpl::FastPairDataEncryptorImpl(
    const fast_pair_encryption::KeyPair& key_pair)
    : secret_key_(key_pair.private_key), public_key_(key_pair.public_key) {}

FastPairDataEncryptorImpl::FastPairDataEncryptorImpl(
    const std::array<uint8_t, kPrivateKeyByteSize>& secret_key)
    : secret_key_(secret_key) {}

FastPairDataEncryptorImpl::~FastPairDataEncryptorImpl() = default;

const std::array<uint8_t, kBlockSizeBytes>
FastPairDataEncryptorImpl::EncryptBytes(
    const std::array<uint8_t, kBlockSizeBytes>& bytes_to_encrypt) {
  return fast_pair_encryption::EncryptBytes(secret_key_, bytes_to_encrypt);
}

const std::optional<std::array<uint8_t, kPublicKeyByteSize>>&
FastPairDataEncryptorImpl::GetPublicKey() {
  return public_key_;
}

void FastPairDataEncryptorImpl::ParseDecryptedResponse(
    const std::vector<uint8_t>& encrypted_response_bytes,
    base::OnceCallback<void(const std::optional<DecryptedResponse>&)>
        callback) {
  if (!ValidateInputSize(encrypted_response_bytes)) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  quick_pair_process::ParseDecryptedResponse(
      std::vector<uint8_t>(secret_key_.begin(), secret_key_.end()),
      encrypted_response_bytes, std::move(callback),
      base::BindOnce(
          &FastPairDataEncryptorImpl::QuickPairProcessStoppedOnResponse,
          weak_ptr_factory_.GetWeakPtr()));
}

void FastPairDataEncryptorImpl::ParseDecryptedPasskey(
    const std::vector<uint8_t>& encrypted_passkey_bytes,
    base::OnceCallback<void(const std::optional<DecryptedPasskey>&)> callback) {
  if (!ValidateInputSize(encrypted_passkey_bytes)) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  quick_pair_process::ParseDecryptedPasskey(
      std::vector<uint8_t>(secret_key_.begin(), secret_key_.end()),
      encrypted_passkey_bytes, std::move(callback),
      base::BindOnce(
          &FastPairDataEncryptorImpl::QuickPairProcessStoppedOnPasskey,
          weak_ptr_factory_.GetWeakPtr()));
}

void FastPairDataEncryptorImpl::QuickPairProcessStoppedOnResponse(
    QuickPairProcessManager::ShutdownReason shutdown_reason) {
  CD_LOG(WARNING, Feature::FP)
      << ": Quick Pair process stopped while decrypting response due to error: "
      << shutdown_reason;
}

void FastPairDataEncryptorImpl::QuickPairProcessStoppedOnPasskey(
    QuickPairProcessManager::ShutdownReason shutdown_reason) {
  CD_LOG(WARNING, Feature::FP)
      << ": Quick Pair process stopped while decrypting passkey due to error: "
      << shutdown_reason;
}

std::vector<uint8_t> FastPairDataEncryptorImpl::CreateAdditionalDataPacket(
    std::array<uint8_t, kNonceSizeBytes> nonce,
    const std::vector<uint8_t>& additional_data) {
  const std::vector<uint8_t> encrypted_additional_data =
      EncryptAdditionalDataWithSecretKey(nonce, additional_data);

  const std::array<uint8_t, fast_pair_encryption::kHmacSizeBytes> hmac =
      fast_pair_encryption::GenerateHmacSha256(secret_key_, nonce,
                                               encrypted_additional_data);

  // Packet Structure (bytes): [First 8 bytes HMAC (8) | Nonce (8) | Encrypted
  // Additional Data (n)].
  int additional_data_packet_size = kHmacAdditionalDataPacketSizeBytes +
                                    kNonceSizeBytes +
                                    encrypted_additional_data.size();
  std::vector<uint8_t> additional_data_packet;
  additional_data_packet.reserve(additional_data_packet_size);
  additional_data_packet.insert(
      additional_data_packet.end(), hmac.begin(),
      std::next(hmac.begin(), kHmacAdditionalDataPacketSizeBytes));
  additional_data_packet.insert(additional_data_packet.end(), nonce.begin(),
                                nonce.end());
  additional_data_packet.insert(additional_data_packet.end(),
                                encrypted_additional_data.begin(),
                                encrypted_additional_data.end());
  additional_data_packet.shrink_to_fit();
  return additional_data_packet;
}

bool FastPairDataEncryptorImpl::VerifyEncryptedAdditionalData(
    const std::array<uint8_t, kHmacVerifyLenBytes> hmacSha256First8Bytes,
    std::array<uint8_t, kNonceSizeBytes> nonce,
    const std::vector<uint8_t>& encrypted_additional_data) {
  const std::array<uint8_t, fast_pair_encryption::kHmacSizeBytes>
      hmac_calculated = fast_pair_encryption::GenerateHmacSha256(
          secret_key_, nonce, encrypted_additional_data);
  CHECK(hmac_calculated.size() >= kHmacVerifyLenBytes);
  for (size_t i = 0; i < kHmacVerifyLenBytes; i++) {
    if (hmacSha256First8Bytes[i] != hmac_calculated[i]) {
      return false;
    }
  }
  return true;
}

std::vector<uint8_t>
FastPairDataEncryptorImpl::EncryptAdditionalDataWithSecretKey(
    std::array<uint8_t, kNonceSizeBytes> nonce,
    const std::vector<uint8_t>& additional_data) {
  return fast_pair_encryption::EncryptAdditionalData(secret_key_, nonce,
                                                     additional_data);
}

}  // namespace quick_pair
}  // namespace ash
